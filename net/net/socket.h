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

#undef SOL_TCP
#define SOL_TCP 1

#undef SO_SNDTIMEO
#define SO_SNDTIMEO     1
#undef SO_RCVTIMEO
#define SO_RCVTIMEO     2
#undef SO_KEEPALIVE
#define SO_KEEPALIVE    3
#undef TCP_KEEPIDLE
#define TCP_KEEPIDLE     4
#undef TCP_KEEPINTVL
#define TCP_KEEPINTVL    5
#undef TCP_KEEPCNT
#define TCP_KEEPCNT      6

typedef uint32_t x_in_addr_t;

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

struct  x_hostent {
    char    * h_name;           /* official name of host */
    char    * h_aliases;        /* alias list */
    short   h_addrtype;         /* host address type */
    short   h_length;           /* length of address */
    char    ** h_addr_list;     /* list of addresses */
};

typedef struct {
    x_in_addr_t * addr_tbl[2];
    x_in_addr_t addr;
    char name[1];
} hostent_extra_t;

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

/**
 * bind
 */
int x_bind(int s, const struct x_sockaddr * addr, x_socklen_t addr_len);

/**
 * listen
 */
int x_listen(int s, int backlog);

/**
 * accept
 */
int x_accept(int s, struct x_sockaddr * addr, x_socklen_t * len);

/**
 * gethostbyname
 */
int x_gethostbyname_r(const char * name, struct x_hostent * ret, char * buf,
        size_t len, struct x_hostent ** result, int * err);

#endif //NET_SOCKET_H
