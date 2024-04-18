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
    int tmo;
    int retry;
    node_t node;
    list_t buf_list;
    netif_t * netif;
} arp_entry_t;

net_err_t arp_init();

/**
 * send a arp request
 */
net_err_t arp_make_request(netif_t * netif, const ipaddr_t * dest);

/**
 * send a gratuitous arp request
 */
net_err_t arp_make_gratuitous(netif_t * netif);

/**
 * arp input data packet handle
 */
net_err_t arp_in(netif_t * netif, pktbuf_t * buf);

/**
 * When an IP packet needs to be sent, it is first looked up from the ARP cache, and if it is found, it is updated with the value in it.
 * If not, a query request is sent, and the packet is added to the waiting queue for the next request before sending the packet.
 */
net_err_t arp_resolve(netif_t * netif, const ipaddr_t * ipaddr, pktbuf_t * buf);

/**
 *  clear the arp cache of given netif
 */
void arp_clear(netif_t * netif);

/**
 * find hwaddr from arp cache table
 */
const uint8_t * arp_find(netif_t * netif, ipaddr_t * ipaddr);

#endif //NET_ARP_H
