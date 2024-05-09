//
// Created by wj on 2024/5/8.
//

#ifndef NET_RAW_H
#define NET_RAW_H

#include "sock.h"
#include "pktbuf.h"
#include "list.h"

typedef struct {
    sock_t base;
    list_t recv_list;
    sock_wait_t rcv_wait;
} raw_t;

net_err_t raw_init();

sock_t * raw_create(int family, int protocol);

/**
 * input raw data packet
 */
net_err_t raw_in(pktbuf_t * pktbuf);

#endif //NET_RAW_H
