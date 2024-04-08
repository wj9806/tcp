//
// Created by wj on 2024/4/8.
//

#include "ether.h"
#include "netif.h"
#include "pktbuf.h"
#include "debug.h"

net_err_t ether_open(struct netif_t * netif)
{
    return NET_ERR_OK;
}

void ether_close(struct netif_t * netif)
{

}

net_err_t ether_in(struct netif_t * netif, pktbuf_t * buf)
{
    return NET_ERR_OK;
}

net_err_t ether_out(struct netif_t * netif, ipaddr_t * dest, pktbuf_t * buf)
{
    return NET_ERR_OK;
}

net_err_t ether_init()
{
    static const link_layer_t link_layer = {
            .type = NETIF_TYPE_ETHER,
            .open = ether_open,
            .close = ether_close,
            .in = ether_in,
            .out = ether_out
    };

    debug_info(DEBUG_ETHER, "init ether");
    net_err_t err = netif_register_layer(NETIF_TYPE_ETHER, &link_layer);
    if (err < 0)
    {
        debug_error(DEBUG_ETHER, "register ether error");
        return err;
    }

    return NET_ERR_OK;
}


