//
// Created by wj on 2024/5/11.
//

#include "udp.h"
#include "mblock.h"
#include "net_cfg.h"

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

sock_t * udp_create(int family, int protocol)
{
    static const sock_ops_t udp_ops = {
            .setopt = sock_setopt,
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
