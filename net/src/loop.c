//
// Created by wj on 2024/4/1.
//

#include "loop.h"

net_err_t loop_open(struct netif_t * netif, void * data)
{
    netif->type = NETIF_TYPE_LOOP;
    return NET_ERR_OK;
}

void loop_close(struct netif_t * netif)
{

}

net_err_t loop_xmit(struct netif_t * netif)
{
    return NET_ERR_OK;
}

static struct netif_ops_t loop_ops = {
        .open = loop_open,
        .close = loop_close,
        .xmit = loop_xmit,
};

net_err_t loop_init()
{
    debug_info(DEBUG_NETIF, "loop init");

    netif_t * netif = netif_open("loop", &loop_ops, (void *)0);
    if (!netif)
    {
        debug_error(DEBUG_NETIF, "open loop error");
        return NET_ERR_NONE;
    }

    ipaddr_t ip, mask;
    ipaddr_from_str(&ip, "127.0.0.1");
    ipaddr_from_str(&mask, "255.0.0.0");

    netif_set_addr(netif, &ip, &mask, (ipaddr_t *)0);
    return NET_ERR_OK;
}
