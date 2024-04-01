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

netif_t * netif_open(const char * dev_name, netif_ops_t * ops, void * ops_data)
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
        mblock_free(&netif_block, netif);
        return (netif_t *) 0;
    }
    err = fixq_init(&netif->out_q, netif->out_q_buf, NETIF_OUTQ_SIZE, LOCKER_THREAD);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif out_q init failed");
        fixq_destroy(&netif->in_q);
        mblock_free(&netif_block, netif);
        return (netif_t *) 0;
    }
    err = ops->open(netif, ops_data);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif ops open err");
        goto error_handle;
    }
    netif->state = NETIF_OPENED;

    if (netif->type == NETIF_TYPE_NONE)
    {
        debug_error(DEBUG_NETIF, "netif type unknown");
        goto error_handle;
    }

    netif->ops = ops;
    netif->ops_data = ops_data;

    list_insert_last(&netif_list, &netif->node);
    return netif;

    error_handle:
    if (netif->state == NETIF_OPENED)
    {
        netif->ops->close(netif);
    }
    fixq_destroy(&netif->in_q);
    fixq_destroy(&netif->out_q);
    mblock_free(&netif_block, netif);
    return (netif_t *)0;
}

net_err_t netif_set_addr(netif_t * netif, ipaddr_t * ip, ipaddr_t * mask, ipaddr_t * gateway)
{
    ipaddr_copy(&netif->ipaddr, ip ? ip : ipaddr_get_any());
    ipaddr_copy(&netif->netmask, mask ? mask : ipaddr_get_any());
    ipaddr_copy(&netif->gateway, gateway ? gateway : ipaddr_get_any());
    return NET_ERR_OK;
}

net_err_t netif_set_hwaddr(netif_t  * netif, const char * hwaddr, int len)
{
    plat_memcpy(netif->hwaddr.addr, hwaddr, len);
    netif->hwaddr.len = len;
    return NET_ERR_OK;
}

void ipaddr_copy(ipaddr_t * dest, const ipaddr_t * src)
{
    if (!dest || !src)
    {
        return;
    }

    dest->type = src->type;
    dest->q_addr = src->q_addr;

}

ipaddr_t * ipaddr_get_any(void)
{
    static const ipaddr_t ipaddr_any = {.type = IPADDR_V4, .q_addr = 0};
    return (ipaddr_t *)&ipaddr_any;
}