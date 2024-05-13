//
// Created by wj on 2024/5/11.
//

#include "udp.h"
#include "mblock.h"
#include "net_cfg.h"
#include "pktbuf.h"
#include "net_api.h"
#include "protocol.h"

static udp_t udp_tbl[UDP_MAX_NR];
static mblock_t udp_mblock;
static list_t udp_list;

net_err_t udp_init()
{
    debug_info(DEBUG_UDP, "udp init");
    list_init(&udp_list);
    mblock_init(&udp_mblock, udp_tbl, sizeof(udp_t), UDP_MAX_NR, LOCKER_NONE);
    return NET_ERR_OK;
}

static int is_port_used(int port)
{
    node_t * node;
    list_for_each(node, &udp_list)
    {
        sock_t * sock = (sock_t *) list_node_parent(node, sock_t, node);
        if (sock->local_port == port)
        {
            return 1;
        }
    }

    return 0;
}

//dynamic alloc available port
static net_err_t alloc_port(sock_t * sock)
{
    static int search_index = NET_PORT_DYN_START;
    for (int i = NET_PORT_DYN_START; i < NET_PORT_DYN_END; ++i)
    {
        int port = search_index++;
        if (!is_port_used(port))
        {
            sock->local_port = port;
            return NET_ERR_OK;
        }
    }

    return NET_ERR_NONE;
}

static net_err_t udp_sendto(struct sock_t * s, const void * buf, size_t len, int flags,
                            const struct x_sockaddr * dest, x_socklen_t dest_len, ssize_t * result_len)
{
    ipaddr_t dest_ip;
    struct x_sockaddr_in * addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip))
    {
        debug_error(DEBUG_UDP, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    uint16_t dport = x_ntohs(addr->sin_port);
    if (s->remote_port && (s->remote_port == dport))
    {
        debug_error(DEBUG_UDP, "invalid port");
        return NET_ERR_PARAM;
    }

    if (!s->local_port && ((s->err = alloc_port(s)) < 0))
    {
        debug_error(DEBUG_UDP, "no port available");
        return NET_ERR_NONE;
    }

    pktbuf_t * pkt_buf = pktbuf_alloc((int)len);
    if (!pkt_buf)
    {
        debug_error(DEBUG_UDP, "no buffer");
        return NET_ERR_NONE;
    }
    net_err_t err = pktbuf_write(pkt_buf, buf, (int)len);
    if (err < 0)
    {
        debug_error(DEBUG_UDP, "copy data error");
        goto end_send_to;
    }

    err = udp_out(&dest_ip, dport, &s->local_ip, s->local_port, pkt_buf);
    if (err < 0)
    {
        debug_error(DEBUG_UDP, "send error");
        goto end_send_to;
    }

    *result_len = (ssize_t)len;
    return NET_ERR_OK;
    end_send_to:
    pktbuf_free(pkt_buf);
    return err;
}

sock_t * udp_create(int family, int protocol)
{
    static const sock_ops_t udp_ops = {
            .setopt = sock_setopt,
            .sendto = udp_sendto,
    };
    udp_t * udp = mblock_alloc(&udp_mblock, -1);
    if (!udp)
    {
        debug_error(DEBUG_UDP, "no udp block");
        return (sock_t *)0;
    }

    net_err_t err = sock_init((sock_t*)udp, family, protocol, &udp_ops);
    if (err < 0)
    {
        debug_error(DEBUG_UDP, "init sock error");
        mblock_free(&udp_mblock, udp);
        return (sock_t *)0;
    }
    list_init(&udp->recv_list);

    udp->base.rcv_wait = &udp->rcv_wait;
    if (sock_wait_init(udp->base.rcv_wait))
    {
        debug_error(DEBUG_UDP, "create rcv wait failed");
        goto create_failed;
    }
    list_insert_last(&udp_list, &udp->base.node);
    return (sock_t*)udp;

    create_failed:
    sock_uninit(&udp->base);
    return (sock_t*)0;
}

net_err_t udp_out(ipaddr_t * dest, uint16_t dport, ipaddr_t * src, uint16_t sport, pktbuf_t * buf)
{
    if (ipaddr_is_any(src))
    {
        rentry_t * rt = rt_find(dest);
        if (rt == (rentry_t *)0)
        {
            debug_error(DEBUG_UDP, "no route");
            return NET_ERR_UNREACHABLE;
        }

        src = &rt->netif->ipaddr;
    }

    net_err_t err = pktbuf_add_header(buf, sizeof(udp_hdr_t), 1);
    if (err < 0)
    {
        debug_error(DEBUG_UDP, "add header failed");
        return NET_ERR_SIZE;
    }

    udp_hdr_t * udp_hdr = (udp_hdr_t *) pktbuf_data(buf);
    udp_hdr->src_port = x_htons(sport);
    udp_hdr->dest_port = x_htons(dport);
    udp_hdr->total_len = x_htons(buf->total_size);
    udp_hdr->checksum = 0;
    udp_hdr->checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_UDP);

    err = ipv4_out(NET_PROTOCOL_UDP, dest, src, buf);
    if (err < 0)
    {
        debug_error(DEBUG_UDP, "udp out err");
        return err;
    }
    return NET_ERR_OK;
}