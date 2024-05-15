//
// Created by wj on 2024/5/7.
//

#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stdint.h>
#include "ipv4.h"
#include "sock.h"

#undef  INADDR_ANY
#define INADDR_ANY              (uint32_t)0x00000000

#undef  AF_INET
#define AF_INET         2

#undef SOCK_RAW
#define SOCK_RAW        0
#undef SOCK_DGRAM
#define SOCK_DGRAM      1
#undef SOCK_STREAM
#define SOCK_STREAM     2

#undef IPPROTO_ICMP
#define IPPROTO_ICMP    1
#undef IPPROTO_TCP
#define IPPROTO_TCP    6
#undef IPPROTO_UDP
#define IPPROTO_UDP    17

#undef SOL_SOCKET
#define SOL_SOCKET 0

#undef SO_SNDTIMEO
#define SO_SNDTIMEO     1
#undef SO_RCVTIMEO
#define SO_RCVTIMEO     2

struct x_timeval {
    long    tv_sec;
    long    tv_usec;
};

struct x_in_addr {
    union {
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };
        uint8_t addr_array[IPV4_ADDR_SIZE];
        #undef s_addr
        uint32_t s_addr;
    };
};

struct x_sockaddr {
    uint8_t sin_len;
    uint8_t sin_family;
    uint8_t sa_data[14];
};

struct x_sockaddr_in {
    uint8_t sin_len;
    uint8_t sin_family;
    uint16_t sin_port;
    struct x_in_addr sin_addr;
    char sin_zero[8];
};

/**
 *
 * @param family protocol family
 * @param type socket type
 */
int x_socket(int family, int type, int protocol);

/**
 * send buf to socket
 */
ssize_t x_sendto(int s, const void * buf, size_t len, int flags, const struct x_sockaddr * dest, x_socklen_t dest_len);

/**
 * send buf to socket
 */
ssize_t x_send(int s, const void * buf, size_t len, int flags);

/**
 * recv buf from socket
 */
ssize_t x_recvfrom(int s, const void * buf, size_t len, int flags, const struct x_sockaddr * src, x_socklen_t * src_len);

/**
 * recv buf from socket
 */
ssize_t x_recv(int s, const void * buf, size_t len, int flags);

/**
 * set socket opt
 */
int x_setsockopt(int s, int level, int optname, const char * optval, int len);

/**
 * close socket
 */
int x_close(int s);

/**
 * connect socket
 */
int x_connect(int s, const struct x_sockaddr * addr, x_socklen_t len);

int x_bind(int s, const struct x_sockaddr * addr, x_socklen_t addr_len);

#endif //NET_SOCKET_H
