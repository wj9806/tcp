//
// Created by wj on 2024/5/8.
//

#include "raw.h"
#include "net_cfg.h"
#include "mblock.h"

static raw_t raw_tbl[RAW_MAX_NR];
static mblock_t raw_mblock;
static list_t raw_list;

net_err_t raw_init()
{
    debug_info(DEBUG_RAW, "raw init");
    list_init(&raw_list);
    mblock_init(&raw_mblock, raw_tbl, sizeof(raw_t), RAW_MAX_NR, LOCKER_NONE);
    return NET_ERR_OK;
}

sock_t * raw_create(int family, int protocol)
{
    static const sock_ops_t raw_ops;
    raw_t * raw = mblock_alloc(&raw_mblock, -1);
    if (!raw)
    {
        debug_error(DEBUG_RAW, "no raw block");
        return (sock_t *)0;
    }

    net_err_t err = sock_init((sock_t*)raw, family, protocol, &raw_ops);
    if (err < 0)
    {
        debug_error(DEBUG_RAW, "init sock error");
        mblock_free(&raw_mblock, raw);
        return (sock_t *)0;
    }
    list_insert_last(&raw_list, &raw->base.node);
    return (sock_t*)raw;
}
