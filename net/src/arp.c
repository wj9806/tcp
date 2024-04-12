//
// Created by wj on 2024/4/12.
//

#include "arp.h"
#include "mblock.h"

static arp_entry_t cache_tbl[ARP_TABLE_SIZE];
static mblock_t cache_block;
static list_t cache_list;

static net_err_t arp_cache_init(void)
{
    list_init(&cache_list);
    net_err_t err = mblock_init(&cache_block, cache_tbl, sizeof(arp_entry_t), ARP_TABLE_SIZE, LOCKER_NONE);
    if (err < 0)
    {
        return err;
    }
    return NET_ERR_OK;
}

net_err_t arp_init()
{
    debug_info(DEBUG_ARP, "init arp");
    net_err_t err = arp_cache_init();
    if (err < 0)
    {
        debug_error(DEBUG_ARP, "init arp cache failed");
        return err;
    }

    return NET_ERR_OK;
}
