//
// Created by wj on 2024/5/15.
//

#ifndef NET_TCP_IN_H
#define NET_TCP_IN_H

#include "tcp.h"
#include "pktbuf.h"

net_err_t tcp_in(pktbuf_t * buf, ipaddr_t * src_ip, ipaddr_t * dest_ip);

#endif //NET_TCP_IN_H
