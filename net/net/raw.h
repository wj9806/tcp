//
// Created by wj on 2024/5/8.
//

#ifndef NET_RAW_H
#define NET_RAW_H

#include "sock.h"

typedef struct {
    sock_t base;
} raw_t;

net_err_t raw_init();

sock_t * raw_create(int family, int protocol);

#endif //NET_RAW_H
