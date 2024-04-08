//
// Created by wj on 2024/4/8.
//

#ifndef NET_ETHER_H
#define NET_ETHER_H

#include <stdint.h>
#include "net_err.h"

#define ETHER_HWA_SIZE          6
#define ETHER_MTU               1500

#pragma pack(1)
//ether data packet header
typedef struct {
    //dest addr
    uint8_t dest[ETHER_HWA_SIZE];
    //src addr
    uint8_t src[ETHER_HWA_SIZE];
    //type
    uint16_t protocol;
} ether_hdr_t;

//ether data packet
typedef struct {
    ether_hdr_t hdr;
    uint8_t data[ETHER_MTU];
} ether_pkt_t;
#pragma pack()

net_err_t ether_init();

#endif //NET_ETHER_H
