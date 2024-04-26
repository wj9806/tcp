//
// Created by wj on 2024/4/24.
//

#ifndef NET_ICMPV4_H
#define NET_ICMPV4_H

#include "net_err.h"
#include "ipaddr.h"
#include "pktbuf.h"

typedef enum {
    ICMPv4_ECHO_REQUEST = 8,
    ICMPv4_ECHO_REPLY = 0,
    ICMPv4_UNREACHABLE = 3,
} icmp_type_t;

typedef enum {
    ICMPv4_ECHO = 0,
    ICMPv4_UNREACHABLE_PORT = 3,
} icmp_code_t;

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

/**
 * output unreachable data packet
 */
net_err_t icmpv4_out_unreachable(ipaddr_t * dest_ip, ipaddr_t * src, uint8_t code, pktbuf_t * buf);

#endif //NET_ICMPV4_H
