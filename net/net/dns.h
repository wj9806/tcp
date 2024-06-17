//
// Created by wj on 2024/6/17.
//

#ifndef NET_DNS_H
#define NET_DNS_H

#include "net_cfg.h"
#include "sys_plat.h"
#include "exmsg.h"

typedef struct {
    char domain_name[DNS_DOMAIN_MAX];
    net_err_t err;
    ipaddr_t ipaddr;
    sys_sem_t wait_sem;
} dns_req_t;

/**
 * alloc dns req struct
 */
dns_req_t * dns_alloc_req(void);

/**
 * free dns req
 */
void dns_free_req(dns_req_t * req);

/**
 * handle dns req
 */
net_err_t dns_req_in(func_msg_t * msg);

#endif //NET_DNS_H
