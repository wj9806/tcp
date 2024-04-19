//
// Created by wj on 2024/4/19.
//

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include "net_err.h"
#include "netif.h"
#include "pktbuf.h"
#include <stdint.h>
#include "net_cfg.h"

#define IPV4_ADDR_SIZE          4
#define NET_VERSION_IPV4        4

#pragma pack(1)
//ipv4 struct
typedef struct {
    union {
        struct {
#if NET_ENDIAN_LITTLE
            //IHL
            uint16_t shdr : 4;
            //Version
            uint16_t version : 4;
            //type of service
            uint16_t tos : 8;
#else
            //Version
            uint16_t version : 4;
            //IHL
            uint16_t shdr : 4;
            //type of service
            uint16_t tos : 8;
#endif
        } ;
        uint16_t shdr_all;
    };
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_all;
    uint8_t ttl;
    uint8_t protocol;
    //header checksum
    uint16_t header_checksum;
    uint8_t src_ip[IPV4_ADDR_SIZE];
    uint8_t dest_ip[IPV4_ADDR_SIZE];
} ipv4_hdr_t;

typedef struct
{
    ipv4_hdr_t hdr;
    uint8_t data[1];
} ipv4_pkt_t;

#pragma pack()

/**
 * init ipv4
 */
net_err_t ipv4_init();

/**
 * input ipv4 data packet
 */
net_err_t ipv4_in(netif_t * netif, pktbuf_t * buf);

/**
 * get the size of ipv4 data packet header
 * @param pkt ipv4 data packet
 * @return size
 */
static inline int ipv4_hdr_size(ipv4_pkt_t * pkt)
{
    return pkt->hdr.shdr * 4;
}

#endif //NET_IPV4_H
