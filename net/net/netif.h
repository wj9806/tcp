//
// Created by wj on 2024/3/28.
//

#ifndef NET_NETIF_H
#define NET_NETIF_H

#include "ipaddr.h"
#include "list.h"
#include "fixq.h"
#include "net_cfg.h"

//hardware addr
typedef struct netif_hwaddr_t {
    uint8_t addr[NETIF_HWADDR_SIZE];
    uint8_t len;
}netif_hwaddr_t;

typedef enum netif_type_t {
    NETIF_TYPE_NONE = 0,
    //ethernet
    NETIF_TYPE_ETHER,
    //loop back
    NETIF_TYPE_LOOP,
} netif_type_t;

/**
 * net interface
 */
typedef struct netif_t {
    //name
    char name[NETIF_NAME_SIZE];
    //hardware addr
    netif_hwaddr_t hwaddr;
    //ipaddr
    ipaddr_t ipaddr;
    //subnet mask
    ipaddr_t netmask;
    //gateway
    ipaddr_t gateway;
    //net if type
    netif_type_t type;
    //mtu
    int mtu;
    //netif state
    enum {
        NETIF_CLOSED,
        NETIF_OPENED,
        NETIF_ACTIVE,
    } state;
    //node
    node_t node;
    //input fixed msg queue
    fixq_t in_q;
    //input buf
    void * in_q_buf[NETIF_INQ_SIZE];
    //output fixed msg queue
    fixq_t out_q;
    //output buf
    void * out_q_buf[NETIF_OUTQ_SIZE];
} netif_t;

#endif //NET_NETIF_H
