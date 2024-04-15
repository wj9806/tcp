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
inline int ipaddr_is_equal(ipaddr_t * ip1, ipaddr_t * ip2)
{
    return ip1->q_addr == ip2->q_addr;
}

/**
 * write ipaddr into buf
 */
void ipaddr_to_buf(const ipaddr_t * src, uint8_t * in_buf);

#endif //NET_IPADDR_H
