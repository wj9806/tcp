//
// Created by wj on 2024/4/12.
//

#ifndef NET_ARP_H
#define NET_ARP_H

#include "ipaddr.h"
#include "ether.h"

typedef struct {
    uint8_t paddr[IPV4_ADDR_SIZE];
    uint8_t hwaddr[ETHER_HWA_SIZE];
    enum {
        NET_ARP_FREE,
        NET_ARP_WAITING,
        NET_ARP_RESOLVED
    } state;
    node_t node;
    list_t buf_list;
    netif_t * netif;
} arp_entry_t;

net_err_t arp_init();

#endif //NET_ARP_H
