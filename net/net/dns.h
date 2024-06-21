//
// Created by wj on 2024/6/17.
//

#ifndef NET_DNS_H
#define NET_DNS_H

#include "net_cfg.h"
#include "sys_plat.h"
#include "exmsg.h"
#include "udp.h"

#define DNS_QUERY_CLASS_INET        1
#define DNS_QUERY_TYPE_A            1

#pragma pack(1)
typedef struct {
    uint16_t id;
    union {
        uint16_t all;
        struct {
#if NET_ENDIAN_LITTLE
            uint16_t rcode : 4;
            uint16_t cd : 1;
            uint16_t ad : 1;
            uint16_t z : 1;
            uint16_t ra : 1;
            uint16_t rd : 1;
            uint16_t tc : 1;
            uint16_t aa : 1;
            uint16_t opcode : 4;
            uint16_t qr : 1;
#else
            uint16_t qr : 1;
            uint16_t opcode : 4;
            uint16_t aa : 1;
            uint16_t tc : 1;
            uint16_t rd : 1;
            uint16_t ra : 1;
            uint16_t z : 1;
            uint16_t ad : 1;
            uint16_t cd : 1;
            uint16_t rcode : 4;
#endif
        };
    } flags;

    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_hdr_t;

typedef struct {
    uint16_t type;
    uint16_t class;
} dns_qfield_t;
#pragma pack()

typedef struct {
    ipaddr_t ipaddr;
    char domain_name[DNS_DOMAIN_MAX];
} dns_entry_t;

typedef struct {
    char domain_name[DNS_DOMAIN_MAX];
    net_err_t err;
    ipaddr_t ipaddr;
    sys_sem_t wait_sem;
} dns_req_t;

/**
 * init dns module
 */
void dns_init(void);

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

/**
 * is dns?
 */
int dns_is_arrive(udp_t * udp);

/**
 * input dns data packet
 */
void dns_in();

#endif //NET_DNS_H
