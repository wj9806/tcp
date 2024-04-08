//
// Created by wj on 2024/3/28.
//

#ifndef NET_NETIF_H
#define NET_NETIF_H

#include "ipaddr.h"
#include "list.h"
#include "fixq.h"
#include "net_cfg.h"
#include "pktbuf.h"

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

    NETIF_TYPE_SIZE,
} netif_type_t;

struct netif_t;

typedef struct netif_ops_t
{
    net_err_t (*open)(struct netif_t * netif, void * data);
    void (*close)(struct netif_t * netif);
    net_err_t (*xmit)(struct netif_t * netif);
} netif_ops_t;

typedef struct {
    netif_type_t type;
    net_err_t (*open)(struct netif_t * netif);
    void (*close)(struct netif_t * netif);
    net_err_t (*in)(struct netif_t * netif, pktbuf_t * buf);
    net_err_t (*out)(struct netif_t * netif, ipaddr_t * dest, pktbuf_t * buf);
} link_layer_t;

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
    /**
     * mtu: Maximum Transmission Unit
     * The maximum number of bytes of data payload that can be transmitted at the data link layer
     */
    int mtu;
    //netif state
    enum {
        NETIF_CLOSED,
        NETIF_OPENED,
        NETIF_ACTIVE,
    } state;

    //net operation
    netif_ops_t * ops;
    void * ops_data;

    //link layer
    const link_layer_t * link_layer;

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

/**
 * init net interface
 */
net_err_t netif_init();

/**
 * open given name's net interface
 * @param dev_name
 * @return
 */
netif_t * netif_open(const char * dev_name, const netif_ops_t * ops, void * ops_data);

/**
 * set netif's ipaddr
 */
net_err_t netif_set_addr(netif_t * netif, ipaddr_t * ip, ipaddr_t * mask, ipaddr_t * gateway);

/**
 * set netif's hardware addr
 */
net_err_t netif_set_hwaddr(netif_t * netif, const char * hwaddr, int len);

/**
 * set netif active state
 */
net_err_t netif_set_active(netif_t * netif);

/**
 * set netif deactive state
 */
net_err_t netif_set_deactive(netif_t * netif);

/**
 * close the given net interface
 */
net_err_t netif_close(netif_t * netif);

/**
 * set default net interface
 */
void netif_set_default(netif_t * netif);

/**
 * write data packet to net interface's in_q
 * @param tmo timeout
 */
net_err_t netif_put_in(netif_t * netif, pktbuf_t * buf, int tmo);

/**
 * get data packet from net interface's in_q
 */
pktbuf_t * netif_get_in(netif_t * netif, int tmo);

/**
 * write data packet to net interface's out_q
 * @param tmo timeout
 */
net_err_t netif_put_out(netif_t * netif, pktbuf_t * buf, int tmo);

/**
 * get data packet from net interface's out_q
 */
pktbuf_t * netif_get_out(netif_t * netif, int tmo);

/**
 * send data packet to given ipaddr
 */
net_err_t netif_out(netif_t * netif, ipaddr_t * ipaddr, pktbuf_t * buf);

/**
 * register link layer
 */
net_err_t netif_register_layer(int type, const link_layer_t * layer);

#endif //NET_NETIF_H
