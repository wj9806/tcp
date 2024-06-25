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
#include "tcp_state.h"

static tcp_t tcp_tbl[TCP_MAX_NR];
static mblock_t tcp_mblock;
static list_t tcp_list;

#if DEBUG_DISP_ENABLED(DEBUG_TCP)
void tcp_show_info(char * msg, tcp_t * tcp)
{
    plat_printf("%s\n", msg);
    plat_printf("   local port: %d, remote port: %d\n", tcp->base.local_port, tcp->base.remote_port);
    plat_printf("   tcp: %p, state: %s\n", tcp, tcp_state_name(tcp->state));
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
#endif

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
        node_t * node;
        list_for_each(node, &tcp_list)
        {
            tcp_t * s = (tcp_t *) list_node_parent(node, sock_t, node);
            if(s->state == TCP_STATE_TIME_WAIT)
            {
                tcp_free(s);
                return (tcp_t *) mblock_alloc(&tcp_mblock, -1);
            }
        }
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
    rentry_t * rt = rt_find(&tcp->base.remote_ip);
    if (rt->netif->mtu == 0 || !ipaddr_is_any(&rt->next_hop))
    {
        tcp->mss = TCP_DEFAULT_MSS;
    }
    else
    {
        tcp->mss = (int) (rt->netif->mtu - sizeof(ipv4_hdr_t) - sizeof(tcp_hdr_t));
    }

    tcp_buf_init(&tcp->snd.buf, tcp->snd.data, TCP_SBUF_SIZE);
    tcp->snd.iss = tcp_get_iss();
    tcp->snd.una = tcp->snd.nxt = tcp->snd.iss;
    tcp_buf_init(&tcp->rcv.buf, tcp->rcv.data, TCP_RBUF_SIZE);
    tcp->rcv.nxt = 0;
    return NET_ERR_OK;
}

static net_err_t tcp_connect(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len)
{
    tcp_t * tcp = (tcp_t*)s;
    const struct x_sockaddr_in * addr_in = (const struct x_sockaddr_in *)addr;

    if (tcp->state != TCP_STATE_CLOSED)
    {
        debug_error(DEBUG_TCP, "tcp is not closed");
        return NET_ERR_STATE;
    }
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
    tcp_set_state(tcp, TCP_STATE_SYN_SENT);
    return NET_ERR_NEED_WAIT;
}

void tcp_free(tcp_t * tcp)
{
    sock_wait_destroy(&tcp->conn.wait);
    sock_wait_destroy(&tcp->rcv.wait);
    sock_wait_destroy(&tcp->snd.wait);
    tcp->state = TCP_STATE_FREE;
    list_remove(&tcp_list, &tcp->base.node);
    mblock_free(&tcp_mblock, tcp);
}

void tcp_clear_parent(tcp_t * tcp)
{
    node_t * node;
    list_for_each(node, &tcp_list)
    {
        tcp_t * child = (tcp_t *) list_node_parent(node, sock_t, node);
        if (child->parent == tcp)
        {
            child->parent = (tcp_t *)0;
        }
    }
}

static net_err_t tcp_close (struct sock_t * s)
{
    tcp_t * tcp = (tcp_t*)s;
    debug_info(DEBUG_TCP, "closing tcp %p: state = %s", tcp, tcp_state_name(tcp->state));
    switch (tcp->state) {
        case TCP_STATE_CLOSED:
            debug_info(DEBUG_TCP, "tcp already closed");
            tcp_free(tcp);
            return NET_ERR_OK;
        case TCP_STATE_LISTEN:
            tcp_clear_parent(tcp);
            tcp_abort(tcp, NET_ERR_CLOSE);
            tcp_free(tcp);
            return NET_ERR_OK;
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECV:
            tcp_abort(tcp, NET_ERR_CLOSE);
            tcp_free(tcp);
            return NET_ERR_OK;
        case TCP_STATE_CLOSE_WAIT:
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_LAST_ACK);
            return NET_ERR_NEED_WAIT;
        case TCP_STATE_ESTABLISHED:
            tcp_send_fin(tcp);
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_1);
            return NET_ERR_NEED_WAIT;
        default:
            debug_error(DEBUG_TCP, "tcp state error");
            return NET_ERR_STATE;
    }
    return NET_ERR_OK;
}

static net_err_t tcp_send(struct sock_t * s, const void * buf, size_t len, int flags, ssize_t * result_len)
{
    tcp_t * tcp = (tcp_t *)s;
    switch (tcp->state) {
        case TCP_STATE_CLOSED:
            debug_error(DEBUG_TCP, "tcp closed");
            return NET_ERR_CLOSE;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_TIME_WAIT:
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSING:
            debug_error(DEBUG_TCP, "tcp closed");
            return NET_ERR_CLOSE;
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_ESTABLISHED:
            break;
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECV:
        default:
            debug_error(DEBUG_TCP, "tcp state error");
            return NET_ERR_STATE;
    }

    int size = tcp_write_sndbuf(tcp, (uint8_t *)buf, (int)len);
    if (size <= 0)
    {
        *result_len = 0;
        return NET_ERR_NEED_WAIT;
    }
    else
    {
        *result_len = size;
        tcp_out_event(tcp, TCP_OEVENT_SEND);
        return NET_ERR_OK;
    }
}

static net_err_t tcp_recv(struct sock_t * s, void * buf, size_t len, int flags, ssize_t * result_len)
{
    tcp_t  * tcp = (tcp_t *)s;
    int need_wait = NET_ERR_NEED_WAIT;
    switch (tcp->state) {
        case TCP_STATE_LAST_ACK:
        case TCP_STATE_CLOSED:
            debug_error(DEBUG_TCP, "tcp already closed");
            return NET_ERR_CLOSE;
        case TCP_STATE_CLOSE_WAIT:
        case TCP_STATE_CLOSING:
            if (tcp_buf_cnt(&tcp->rcv.buf) == 0)
            {
                return NET_ERR_CLOSE;
            }
            need_wait = NET_ERR_OK;
            break;
        case TCP_STATE_FIN_WAIT_1:
        case TCP_STATE_FIN_WAIT_2:
        case TCP_STATE_ESTABLISHED:
            break;
        case TCP_STATE_LISTEN:
        case TCP_STATE_SYN_SENT:
        case TCP_STATE_SYN_RECV:
        case TCP_STATE_TIME_WAIT:
            debug_error(DEBUG_TCP, "tcp state error");
            return NET_ERR_STATE;
        default:
            break;
    }
    *result_len = 0;
    int cnt = tcp_buf_read_rcv(&tcp->rcv.buf, buf, (int)len);
    if (cnt > 0)
    {
        *result_len = cnt;
        return NET_ERR_OK;
    }
    return need_wait;
}

static net_err_t tcp_setopt(struct sock_t * s, int level, int optname, const char * optval, int optlen)
{
    net_err_t err = sock_setopt(s, level, optname, optval, optlen);
    if (err == NET_ERR_OK)
    {
        return err;
    }
    else if (err < 0 && (err != NET_ERR_UNKNOWN))
    {
        return err;
    }

    tcp_t * tcp = (tcp_t *)s;
    if (level == SOL_SOCKET)
    {
        if (optname == SO_KEEPALIVE)
        {
            if (optlen != sizeof(int))
            {
                debug_error(DEBUG_TCP, "param size error");
                return NET_ERR_PARAM;
            }
            tcp_keepalive_start(tcp, *(int *)optval);
            return NET_ERR_OK;
        }
        return NET_ERR_PARAM;
    }
    else if(level == SOL_TCP)
    {
        switch (optname) {
            case TCP_KEEPIDLE:
                if (optlen != sizeof(int))
                {
                    debug_error(DEBUG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_idle = *(int *)optval;
                tcp_keepalive_restart(tcp);
                break;
            case TCP_KEEPINTVL:
                if (optlen != sizeof(int))
                {
                    debug_error(DEBUG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_intvl = *(int *)optval;
                tcp_keepalive_restart(tcp);
                break;
            case TCP_KEEPCNT:
                if (optlen != sizeof(int))
                {
                    debug_error(DEBUG_TCP, "param size error");
                    return NET_ERR_PARAM;
                }
                tcp->conn.keep_cnt = *(int *)optval;
                tcp_keepalive_restart(tcp);
                break;
            default:
                debug_error(DEBUG_TCP, "unknown param");
                return NET_ERR_PARAM;
        }
    }

    return NET_ERR_OK;
}

static net_err_t tcp_bind(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len)
{
    tcp_t * tcp = (tcp_t *)s;

    if (tcp->state != TCP_STATE_CLOSED)
    {
        debug_error(DEBUG_TCP, "state error");
        return NET_ERR_STATE;
    }

    if (s->local_port != NET_PORT_EMPTY)
    {
        debug_error(DEBUG_TCP, "tcp already bind");
        return NET_ERR_PARAM;
    }

    const struct x_sockaddr_in * addr_in = (const struct x_sockaddr_in *) addr;
    if (addr_in->sin_port == NET_PORT_EMPTY)
    {
        debug_error(DEBUG_TCP, "port empty");
        return NET_ERR_PARAM;
    }

    ipaddr_t local_ip;
    ipaddr_from_buf(&local_ip, (const uint8_t *) &addr_in->sin_addr);
    if (!ipaddr_is_any(&local_ip))
    {
        rentry_t * rt = rt_find(&local_ip);
        if (rt == (rentry_t *)0)
        {
            debug_error(DEBUG_TCP, "ipaddr error");
            return NET_ERR_PARAM;
        }

        if (!ipaddr_is_equal(&local_ip, &rt->netif->ipaddr))
        {
            debug_error(DEBUG_TCP, "ipaddr error");
            return NET_ERR_PARAM;
        }
    }

    node_t * node;
    list_for_each(node, &tcp_list)
    {
        sock_t * curr = (sock_t*) list_node_parent(node, sock_t , node);
        if (curr == s)
        {
            continue;
        }
        if (curr->remote_port != NET_PORT_EMPTY)
        {
            continue;
        }
        if (ipaddr_is_equal(&curr->local_ip, &local_ip) && (curr->local_port == addr_in->sin_port))
        {
            debug_error(DEBUG_TCP, "ipaddr and port already bound");
            return NET_ERR_PARAM;
        }
    }

    ipaddr_copy(&s->local_ip, &local_ip);
    s->local_port = x_ntohs(addr_in->sin_port);
    return NET_ERR_OK;
}

static net_err_t tcp_listen(struct sock_t * s, int backlog)
{
    tcp_t * tcp = (tcp_t *)s;
    if (tcp->state != TCP_STATE_CLOSED)
    {
        debug_error(DEBUG_TCP, "tcp state error");
        return NET_ERR_STATE;
    }

    tcp->state = TCP_STATE_LISTEN;
    tcp->conn.backlog = backlog;
    return NET_ERR_OK;
}

static net_err_t tcp_accept(struct sock_t * s, struct x_sockaddr * addr, x_socklen_t * len, struct sock_t ** client)
{
    node_t * node;
    list_for_each(node, &tcp_list)
    {
        sock_t * sock = list_node_parent(node, sock_t, node);
        tcp_t * tcp = (tcp_t *) sock;

        if (sock == s)
        {
            continue;
        }

        if (tcp->parent != (tcp_t *)s)
        {
            continue;
        }

        if (tcp->flags.inactive && (tcp->state == TCP_STATE_ESTABLISHED))
        {
            struct x_sockaddr_in* addr_in = (struct x_sockaddr_in*) addr;
            plat_memset(addr_in, 0, *len);
            addr_in->sin_family = AF_INET;
            addr_in->sin_port = x_htons(tcp->base.remote_port);
            ipaddr_to_buf(&tcp->base.remote_ip, (uint8_t*)&addr_in->sin_addr.s_addr);

            tcp->flags.inactive = 0;
            *client = sock;
            return NET_ERR_OK;
        }
    }
    return NET_ERR_NEED_WAIT;
}

static void tcp_destroy(struct sock_t * s)
{
    tcp_t * tcp = (tcp_t *)s;

    if (tcp->state == TCP_STATE_TIME_WAIT)
    {
        return;
    }
    tcp_kill_all_timers(tcp);
    tcp_free(tcp);
}

static tcp_t * tcp_alloc(int wait, int family, int protocol)
{
    //tcp function table
    static const sock_ops_t tcp_ops = {
            .connect = tcp_connect,
            .close = tcp_close,
            .send = tcp_send,
            .recv = tcp_recv,
            .setopt = tcp_setopt,
            .bind = tcp_bind,
            .listen = tcp_listen,
            .accept = tcp_accept,
            .destroy = tcp_destroy,
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
    tcp->state = TCP_STATE_CLOSED;
    tcp->flags.keep_enable = 0;
    tcp->conn.keep_idle = TCP_KEEPALIVE_TIME;
    tcp->conn.keep_intvl = TCP_KEEPALIVE_INTVL;
    tcp->conn.keep_cnt = TCP_KEEPALIVE_PROBES;

    tcp->snd.ostate = TCP_OSTATE_IDLE;
    tcp->snd.rto = TCP_INIT_RTO;
    tcp->snd.rexmit_max = TCP_INIT_RETRIES;

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

tcp_t * tcp_find(ipaddr_t * local_ip, uint16_t local_port, ipaddr_t * remote_ip, uint16_t remote_port)
{
    sock_t* match = (sock_t*)0;

    node_t* node;
    list_for_each(node, &tcp_list) {
        sock_t* s = list_node_parent(node, sock_t, node);

        if (ipaddr_is_equal(&s->local_ip, local_ip) && (s->local_port == local_port) &&
            ipaddr_is_equal(&s->remote_ip, remote_ip) && (s->remote_port == remote_port)) {
            return (tcp_t*)s;
        }

        tcp_t * tcp = (tcp_t *)s;
        if ((tcp->state == TCP_STATE_LISTEN) && (s->local_port == local_port)) {
            if (!ipaddr_is_any(&s->local_ip) && !ipaddr_is_equal(&s->local_ip, local_ip)) {
                continue;
            }

            if (s->local_port == local_port) {
                match = s;
            }
        }
    }

    return (tcp_t*)match;
}

net_err_t tcp_abort(tcp_t * tcp, net_err_t err)
{
    tcp_kill_all_timers(tcp);
    tcp_set_state(tcp, TCP_STATE_CLOSED);
    sock_wakeup(&tcp->base, SOCK_WAIT_ALL, err);
    return NET_ERR_OK;
}

void tcp_read_options(tcp_t * tcp, tcp_hdr_t * tcp_hdr)
{
    uint8_t * opt_start = (uint8_t *)tcp_hdr + sizeof(tcp_hdr_t);
    uint8_t * opt_end = opt_start + (tcp_hdr_size(tcp_hdr) - sizeof(tcp_hdr_t));

    if (opt_end <= opt_start)
    {
        return;
    }

    while (opt_start < opt_end)
    {
        switch (opt_start[0]) {
            case TCP_OPT_MSS:
                tcp_opt_mss_t * opt = (tcp_opt_mss_t *)opt_start;
                if (opt->length == 4)
                {
                    uint16_t mss = x_ntohs(opt->mss);
                    if (mss < tcp->mss)
                    {
                        tcp->mss = mss;
                    }
                    opt_start += opt->length;
                }
                else
                {
                    opt_start++;
                }
                break;
            case TCP_OPT_NOP:
                opt_start++;
                break;

            case TCP_OPT_END:
                return;
            default:
                opt_start++;
                break;
        }
    }
}

int tcp_rcv_window(tcp_t * tcp)
{
    int windows = tcp_buf_free_cnt(&tcp->rcv.buf);
    return windows;

}

static void tcp_alive_tmo(struct net_timer_t * timer, void * arg)
{
    tcp_t * tcp = (tcp_t *) arg;

    if (++tcp->conn.keep_retry <= tcp->conn.keep_cnt)
    {
        //send keepalive packet
        tcp_send_keepalive(tcp);

        net_timer_remove(&tcp->conn.keep_timer);
        net_timer_add(&tcp->conn.keep_timer, "tcp-keepalive-timer", tcp_alive_tmo, tcp, tcp->conn.keep_intvl * 1000, 0);
        debug_info(DEBUG_TCP, "tcp alive tmo, retry: %d", tcp->conn.keep_cnt);
    }
    else
    {
        //send rst
        tcp_send_reset_for_tcp(tcp);
        tcp_abort(tcp, NET_ERR_CLOSE);
        debug_error(DEBUG_TCP, "tcp alive tmo, give up");
    }
}

static void keepalive_start_timer(tcp_t * tcp)
{
    net_timer_add(&tcp->conn.keep_timer, "tcp-keepalive-timer", tcp_alive_tmo, tcp, tcp->conn.keep_idle * 1000, 0);
}

void tcp_keepalive_start(tcp_t * tcp, int run)
{
    if (!run && tcp->flags.keep_enable)
    {
        net_timer_remove(&tcp->conn.keep_timer);
    }
    else if(run && !tcp->flags.keep_enable)
    {
        keepalive_start_timer(tcp);
    }

    tcp->flags.keep_enable = run;
}

void tcp_keepalive_restart(tcp_t * tcp)
{
    if (tcp->flags.keep_enable)
    {
        net_timer_remove(&tcp->conn.keep_timer);
        keepalive_start_timer(tcp);
        tcp->conn.keep_retry = 0;
    }
}

void tcp_kill_all_timers(tcp_t * tcp)
{
    net_timer_remove(&tcp->snd.timer);
    net_timer_remove(&tcp->conn.keep_timer);
}

int tcp_backlog_count(tcp_t * tcp)
{
    int count = 0;

    node_t * node;
    list_for_each(node, &tcp_list)
    {
        tcp_t * child = (tcp_t *)list_node_parent(node, sock_t, node);
        if(child->parent == tcp && child->flags.inactive)
        {
            count++;
        }
    }
    return count;
}

tcp_t * tcp_create_child(tcp_t* tcp, tcp_seg_t * seg)
{
    tcp_t * child = tcp_alloc(0, tcp->base.family, tcp->base.protocol);
    if (!child)
    {
        debug_error(DEBUG_TCP, "no child tcp");
        return (tcp_t *)0;
    }

    ipaddr_copy(&child->base.local_ip, &seg->local_ip);
    ipaddr_copy(&child->base.remote_ip, &seg->remote_ip);
    child->base.local_port = seg->hdr->dport;
    child->base.remote_port = seg->hdr->sport;
    child->parent = tcp;
    child->flags.irs_valid = 1;
    child->flags.inactive = 1;

    tcp_init_connect(child);
    child->rcv.iss = seg->seq;
    child->rcv.nxt = child->rcv.iss + 1;
    tcp_read_options(child, seg->hdr);

    tcp_insert(child);
    return child;
}