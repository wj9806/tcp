//
// Created by wj on 2024/5/11.
//

#include "udp.h"
#include "mblock.h"
#include "net_cfg.h"
#include "pktbuf.h"
#include "net_api.h"

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

    //
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
