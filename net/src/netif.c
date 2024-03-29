//
// Created by wj on 2024/3/29.
//
#include "netif.h"
#include "mblock.h"

static netif_t netif_buffer[NETIF_DEV_CNT];
static mblock_t netif_block;
static list_t netif_list;
static netif_t * netif_default;

net_err_t netif_init()
{
    debug_info(DEBUG_NETIF, "init netif");
    list_init(&netif_list);
    mblock_init(&netif_block, netif_buffer, sizeof(netif_t ), NETIF_DEV_CNT, LOCKER_NONE);

    netif_default = (netif_t *) 0;
    return NET_ERR_OK;
}

netif_t * netif_open(const char * dev_name)
{
    netif_t * netif = (netif_t *) mblock_alloc(&netif_block, - 1);
    if (!netif)
    {
        debug_error(DEBUG_NETIF, "no netif");
        return (netif_t *) 0;
    }
    ipaddr_set_any(&netif->ipaddr);
    ipaddr_set_any(&netif->netmask);
    ipaddr_set_any(&netif->gateway);

    plat_strncpy(netif->name, dev_name, NETIF_NAME_SIZE);
    netif->name[NETIF_NAME_SIZE - 1] = '\0';
    netif->type = NETIF_TYPE_NONE;
    netif->mtu = 0;
    node_init(&netif->node);
    net_err_t err = fixq_init(&netif->in_q, netif->in_q_buf, NETIF_INQ_SIZE, LOCKER_THREAD);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif in_q init failed");
        return (netif_t *) 0;
    }
    err = fixq_init(&netif->out_q, netif->out_q_buf, NETIF_OUTQ_SIZE, LOCKER_THREAD);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif out_q init failed");
        fixq_destroy(&netif->in_q);
        return (netif_t *) 0;
    }
    netif->state = NETIF_OPENED;

    list_insert_last(&netif_list, &netif->node);
    return netif;
}
