//
// Created by wj on 2024/5/14.
//

#ifndef NET_TCP_H
#define NET_TCP_H

#include "net_err.h"
#include "sock.h"

typedef struct {
    sock_t base;
} tcp_t;

net_err_t tcp_init();

/**
 * create tcp
 */
sock_t * tcp_create(int family, int protocol);

#endif //NET_TCP_H
