//
// Created by wj on 2024/5/14.
//

#include "tcp.h"
#include "debug.h"
#include "mblock.h"

static tcp_t tcp_tbl[TCP_MAX_NR];
static mblock_t tcp_mblock;
static list_t tcp_list;

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

static tcp_t * tcp_alloc(int wait, int family, int protocol)
{
    static const sock_ops_t tcp_ops;

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

    return tcp;
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
