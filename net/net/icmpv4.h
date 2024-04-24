//
// Created by wj on 2024/4/24.
//

#ifndef NET_ICMPV4_H
#define NET_ICMPV4_H

#include "net_err.h"
#include "ipaddr.h"
#include "pktbuf.h"

net_err_t icmpv4_init();

net_err_t icmpv4_in(ipaddr_t * src_ip, ipaddr_t * netif_ip, pktbuf_t * buf);

#endif //NET_ICMPV4_H
