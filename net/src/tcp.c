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
