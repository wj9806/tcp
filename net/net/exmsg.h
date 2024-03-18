//
// Created by xsy on 2024/3/14.
//

#ifndef NET_EXMSG_H
#define NET_EXMSG_H

#include "net_err.h"
#include "list.h"

typedef struct exmsg_t {
    node_t node;
    enum
    {
        NET_EXMSG_NETIF_IN,
    } type;

    int id;
} exmsg_t;

net_err_t exmsg_init(void);
net_err_t exmsg_start(void);
net_err_t exmsg_netif_in();

#endif //NET_EXMSG_H
