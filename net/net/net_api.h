//
// Created by wj on 2024/5/7.
//

#ifndef NET_NET_API_H
#define NET_NET_API_H

#include <stdint.h>
#include "tools.h"
#include "socket.h"

#undef  sockaddr_in
#define sockaddr_in x_sockaddr_in
#undef sockaddr
#define sockaddr    x_sockaddr
#undef socklen_t
#define socklen_t   x_socklen_t
#undef timeval
#define timeval     x_timeval

#define socket(family, type, protocol)                      x_socket(family, type, protocol)
#define sendto(s, buf, len, flags, dest, dlen)              x_sendto(s, buf, len, flags, dest, dlen)
#define recvfrom(s, buf, len, flags, src, slen)             x_recvfrom(s, buf, len, flags, src, slen)
#define setsockopt(s, level, optname, optval, len)          x_setsockopt(s, level, optname, optval, len)
#define close(s)                                            x_close(s)
#define connect(s, addr, addr_len)                          x_connect(s, addr, addr_len)
#define send(s, buf, len, flags)                            x_send(s, buf, len, flags)
#define recv(s, buf, len, flags)                            x_recv(s, buf, len, flags)
#define bind(s, addr, len)                                  x_bind(s, addr, len)
#define listen(s, backlog)                                  x_listen(s, backlog)
#define accept(s, addr, len)                                x_accept(s, addr, len)

#define x_htons(v)        swap_u16(v)
#define x_ntohs(v)        swap_u16(v)
#define x_htonl(v)        swap_u32(v)
#define x_ntohl(v)        swap_u32(v)

#undef htons
#define htons(v)          x_htons(v)

#undef ntohs
#define ntohs(v)          x_ntohs(v)

#undef htonl
#define htonl(v)          x_htonl(v)

#undef ntohl
#define ntohl(v)          x_ntohl(v)

#define hostent           x_hostent

/**
 * convert addr to string
 */
char * x_inet_ntoa(struct x_in_addr in);

/**
 * convert string to addr
 */
uint32_t x_inet_addr(const char * str);

/**
 * convert string to x_in_addr
 */
int x_inet_pton(int family, const char * strptr, void * addrptr);

const char * x_inet_ntop(int family, const void * addrptr, char * strptr, size_t len);

#define inet_ntoa(in)                               x_inet_ntoa(in)
#define inet_addr(str)                              x_inet_addr(str)
#define inet_pton(family, strptr, addrptr)          x_inet_pton(family, strptr, addrptr)
#define inet_ntop(family, addrptr, strptr, len)     x_inet_ntop(family, addrptr, strptr, len)

#define gethostbyname_r(dest, ret, buf, len, result, err)   x_gethostbyname_r(dest, ret, buf, len, result, err)

#endif //NET_NET_API_H
