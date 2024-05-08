//
// Created by wj on 2024/5/8.
//

#ifndef NET_SOCK_H
#define NET_SOCK_H

#include "net_err.h"

typedef struct {
    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED
    } state;
} x_socket_t;

net_err_t socket_init();

#endif //NET_SOCK_H
