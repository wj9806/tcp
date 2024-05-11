//
// Created by wj on 2024/5/11.
//

#ifndef NET_UDP_H
#define NET_UDP_H

#include "sock.h"

typedef struct {
    sock_t base;
    list_t recv_list;
    sock_wait_t rcv_wait;
} udp_t;

/**
 * init udp module
 */
net_err_t udp_init();

/**
 * create udp sock
 */
sock_t * udp_create(int family, int protocol);

#endif //NET_UDP_H
