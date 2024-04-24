//
// Created by wj on 2024/4/24.
//

#ifndef NET_ICMPV4_H
#define NET_ICMPV4_H

#include "net_err.h"
#include "ipaddr.h"
#include "pktbuf.h"

#pragma pack(1)

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} icmpv4_hdr_t;

typedef struct {
    icmpv4_hdr_t hdr;
    union {
        uint32_t reverse;

    };
    uint8_t data[1];
} icmp_pkt_t;
#pragma pack()

net_err_t icmpv4_init();

/**
 * input icmpv4 data packet
 */
net_err_t icmpv4_in(ipaddr_t * src_ip, ipaddr_t * netif_ip, pktbuf_t * buf);

#endif //NET_ICMPV4_H
