//
// Created by wj on 2024/5/14.
//

#include "tcp.h"
#include "debug.h"
#include "mblock.h"
#include "socket.h"
#include "tools.h"
#include "protocol.h"
#include "tcp_out.h"

static tcp_t tcp_tbl[TCP_MAX_NR];
static mblock_t tcp_mblock;
static list_t tcp_list;

#if DEBUG_DISP_ENABLED(DEBUG_TCP)
void tcp_show_info(char * msg, tcp_t * tcp)
{
    plat_printf("%s\n", msg);
    plat_printf("   local port: %d, remote port: %d\n", tcp->base.local_port, tcp->base.remote_port);
}

void tcp_show_pkt(char * msg, tcp_hdr_t * tcp_hdr, pktbuf_t * buf)
{
    plat_printf("%s\n", msg);
    plat_printf("   sport: %d, dport: %d\n", tcp_hdr->sport, tcp_hdr->dport);
    plat_printf("   seq: %u, ack: %u, win: %u\n", tcp_hdr->seq, tcp_hdr->ack, tcp_hdr->win);
    plat_printf("   flags:");
    if (tcp_hdr->f_syn)
    {
        plat_printf("   syn");
    }
    if (tcp_hdr->f_rst)
    {
        plat_printf("   rst");
    }
    if (tcp_hdr->f_ack)
    {
        plat_printf("   ack");
    }
    if (tcp_hdr->f_psh)
    {
        plat_printf("   psh");
    }
    if (tcp_hdr->f_fin)
    {
        plat_printf("   fin");
    }
    plat_printf("\n   len=%d\n", buf->total_size - tcp_hdr_size(tcp_hdr));
}

#endif

void tcp_show_list(void)
{
    plat_printf("--------------tcp list--------------\n");
    node_t * node;
    list_for_each(node, &tcp_list)
    {
        tcp_t * tcp = (tcp_t *) list_node_parent(node, sock_t, node);
        tcp_show_info("", tcp);
    }
}


net_err_t tcp_init()
{
    debug_info(DEBUG_TCP, "tcp init");
    list_init(&tcp_list);
    mblock_init(&tcp_mblock, tcp_tbl, sizeof(tcp_t), TCP_MAX_NR, LOCKER_NONE);
    return NET_ERR_OK;
}

static tcp_t * tcp_get_free(int wait)
{
    tcp_t * tcp = mblock_alloc(&tcp_mblock, wait ? 0 : -1);
    if (!tcp)
    {
        debug_error(DEBUG_TCP, "no tcp sock");
        return (tcp_t *)0;
    }
    return tcp;
}

/** alloc port for tcp connection **/
static int tcp_alloc_port()
{
#if 1
    srand((unsigned int)time(NULL));
    int num = rand() % 1000;
    int search_index = num + NET_PORT_DYN_START;
#else
    static int search_index = NET_PORT_DYN_START;
#endif
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; ++i) {
        node_t * node;
        list_for_each(node, &tcp_list)
        {
            sock_t * sock = list_node_parent(node, sock_t, node);
            if (sock->local_port == search_index)
            {
                break;
            }
        }
        if (++search_index >= NET_PORT_DYN_END)
        {
            search_index = NET_PORT_DYN_START;
        }
        if (!node)
        {
            return search_index;
        }
    }

    return NET_PORT_EMPTY;
}

static uint32_t tcp_get_iss()
{
    static uint32_t seq = 0;
    return ++seq;
}

static net_err_t tcp_init_connect(tcp_t * tcp)
{
    tcp->snd.iss = tcp_get_iss();
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss;

    tcp->rcv.nxt = 0;

    return NET_ERR_OK;
}

static net_err_t tcp_connect(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len)
{
    tcp_t * tcp = (tcp_t*)s;
    const struct x_sockaddr_in * addr_in = (const struct x_sockaddr_in *)addr;

    ipaddr_from_buf(&s->remote_ip, (const uint8_t *)&addr_in->sin_addr.addr_array);
    s->remote_port = x_ntohs(addr_in->sin_port);

    if (s->local_port == NET_PORT_EMPTY)
    {
        int port = tcp_alloc_port();
        if (port == NET_PORT_EMPTY)
        {
            debug_error(DEBUG_TCP, "alloc tcp port failed");
            return NET_ERR_NONE;
        }

        s->local_port = port;
    }

    if (ipaddr_is_any(&s->local_ip))
    {
        /** find route table by remote ip **/
        rentry_t * rt = rt_find(&s->remote_ip);
        if (rt == (rentry_t *)0)
        {
            debug_error(DEBUG_TCP, "cannot find rentry");
            return NET_ERR_UNREACHABLE;
        }

        ipaddr_copy(&s->local_ip, &rt->netif->ipaddr);
    }

    net_err_t err = tcp_init_connect(tcp);
    if (err < 0)
    {
        debug_error(DEBUG_TCP, "init tcp conn failed");
        return err;
    }

    if ((err = tcp_send_syn(tcp)) < 0)
    {
        debug_error(DEBUG_TCP, "send syn failed");
        return err;
    }
    return NET_ERR_NEED_WAIT;
}

static net_err_t tcp_close (struct sock_t * s)
{
    return NET_ERR_OK;
}

static tcp_t * tcp_alloc(int wait, int family, int protocol)
{
    //tcp function table
    static const sock_ops_t tcp_ops = {
            .connect = tcp_connect,
            .close = tcp_close,
    };

    tcp_t * tcp = tcp_get_free(wait);
    if (!tcp)
    {
        debug_error(DEBUG_TCP, "no tcp sock");
        return (tcp_t *)0;
    }

    plat_memset(tcp, 0, sizeof(tcp_t));

    net_err_t err = sock_init((sock_t*)tcp, family, protocol, &tcp_ops);
    if (err < 0)
    {
        debug_error(DEBUG_TCP, "sock init failed");
        mblock_free(&tcp_mblock, tcp);
        return (tcp_t *)0;
    }

    if(sock_wait_init(&tcp->conn.wait))
    {
        debug_error(DEBUG_TCP, "create conn.wait failed.");
        goto alloc_failed;
    }
    tcp->base.conn_wait = &tcp->conn.wait;

    if(sock_wait_init(&tcp->snd.wait))
    {
        debug_error(DEBUG_TCP, "create snd.wait failed.");
        goto alloc_failed;
    }
    tcp->base.snd_wait = &tcp->snd.wait;

    if(sock_wait_init(&tcp->rcv.wait))
    {
        debug_error(DEBUG_TCP, "create rcv.wait failed.");
        goto alloc_failed;
    }
    tcp->base.rcv_wait = &tcp->rcv.wait;

    return tcp;

alloc_failed:
    if(tcp->base.conn_wait)
    {
        sock_wait_destroy(tcp->base.conn_wait);
    }
    if(tcp->base.snd_wait)
    {
        sock_wait_destroy(tcp->base.snd_wait);
    }
    if(tcp->base.rcv_wait)
    {
        sock_wait_destroy(tcp->base.rcv_wait);
    }
    mblock_free(&tcp_mblock, tcp);
    return (tcp_t *)0;
}

static void tcp_insert(tcp_t * tcp)
{
    list_insert_last(&tcp_list, &tcp->base.node);
    assert(tcp_list.count <= TCP_MAX_NR, "tcp count err");
}

sock_t * tcp_create(int family, int protocol)
{
    tcp_t * tcp = tcp_alloc(1, family, protocol);
    if (!tcp)
    {
        debug_error(DEBUG_TCP, "alloc tcp failed");
        return (sock_t *)0;
    }

    tcp_insert(tcp);
    return (sock_t *)tcp;
}
