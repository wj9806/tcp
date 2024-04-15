//
// Created by wj on 2024/4/12.
//

#ifndef NET_ARP_H
#define NET_ARP_H

#include "ipaddr.h"
#include "ether.h"

#define ARP_HW_ETHER        1
#define ARP_REQUEST         1
#define ARP_REPLY           2

#pragma pack(1)
// arp data packet type
typedef struct {
    //hardware type
    uint16_t htype;
    //protocol type
    uint16_t ptype;
    //hardware len
    uint8_t hwlen;
    //ip addr len
    uint8_t plen;
    //opcode
    uint16_t opcode;
    uint8_t sender_hwaddr[ETHER_HWA_SIZE];
    uint8_t sender_paddr[IPV4_ADDR_SIZE];
    uint8_t target_hwaddr[ETHER_HWA_SIZE];
    uint8_t target_paddr[IPV4_ADDR_SIZE];
} arp_pkt_t;
#pragma pack()

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

//send a arp request
net_err_t arp_make_request(netif_t * netif, const ipaddr_t * dest);

#endif //NET_ARP_H
