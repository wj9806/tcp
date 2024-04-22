//
// Created by wj on 2024/3/28.
//

#ifndef NET_IPADDR_H
#define NET_IPADDR_H
#include <stdint.h>
#include "net_err.h"

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

/**
 * convert string to ipaddr_t
 */
net_err_t ipaddr_from_str(ipaddr_t * dest, const char * str);

/**
 * copy addr from dest to src
 */
void ipaddr_copy(ipaddr_t * dest, const ipaddr_t * src);

/**
 * get default ipaddr
 */
ipaddr_t * ipaddr_get_any(void);

/**
 * @return ip1 is equal ip2
 */
inline int ipaddr_is_equal(const ipaddr_t * ip1, const ipaddr_t * ip2)
{
    return ip1->q_addr == ip2->q_addr;
}

/**
 * write ipaddr into buf
 */
void ipaddr_to_buf(const ipaddr_t * src, uint8_t * in_buf);

/**
 * convert ip_buf to ipaddr
 */
void ipaddr_from_buf(ipaddr_t * dest, const uint8_t * ip_buf);

/**
 * is local broadcast ipaddr
 */
int ipaddr_is_local_broadcast(const ipaddr_t * ipaddr);

/**
 * is direct broadcast ipaddr
 */
int ipaddr_is_direct_broadcast(const ipaddr_t * ipaddr, const ipaddr_t * netmask);

/**
 * get ipaddr net component
 */
ipaddr_t ipaddr_get_net(const ipaddr_t * ipaddr, const ipaddr_t * netmask);

/**
 * is can reached
 */
int ipaddr_is_match(const ipaddr_t* dest, const ipaddr_t * src, const ipaddr_t * netmask);

#endif //NET_IPADDR_H
