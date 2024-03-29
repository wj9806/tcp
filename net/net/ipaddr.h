//
// Created by wj on 2024/3/28.
//

#ifndef NET_IPADDR_H
#define NET_IPADDR_H
#include <stdint.h>

#define IPV4_ADDR_SIZE          4

typedef struct ipaddr_t {
    enum {
        IPADDR_V4
    } type;

    union {
        uint32_t q_addr;
        uint8_t addr[IPV4_ADDR_SIZE];
    };

} ipaddr_t;

/**
 * Set a default value for the ip address
 */
void ipaddr_set_any(ipaddr_t * ip);

#endif //NET_IPADDR_H
