//
// Created by wj on 2024/4/19.
//

#include "ipv4.h"
#include "debug.h"

net_err_t ipv4_init()
{
    debug_info(DEBUG_IP, "init ipv4");

    return NET_ERR_OK;
}

net_err_t ipv4_in(netif_t * netif, pktbuf_t * buf)
{

    pktbuf_free(buf);
    return NET_ERR_OK;
}
