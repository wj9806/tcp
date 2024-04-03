//
// Created by xsy on 2024/3/14.
//

#ifndef NET_NETIF_PCAP_H
#define NET_NETIF_PCAP_H

#include "net_err.h"
#include "netif.h"

typedef struct {
    const char * ip;

    const uint8_t * hwaddr;
} pcap_data_t;

extern const struct netif_ops_t netdev_ops;

net_err_t netif_pcap_open(struct netif_t * netif, void * data);


#endif //NET_NETIF_PCAP_H
