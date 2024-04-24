//
// Created by wj on 2024/4/24.
//

#include "icmpv4.h"
#include "debug.h"

net_err_t icmpv4_init()
{
    debug_info(DEBUG_ICMP, "icmp init");
    return NET_ERR_OK;
}

net_err_t icmpv4_in(ipaddr_t * src_ip, ipaddr_t * netif_ip, pktbuf_t * buf)
{

}
