//
// Created by wj on 2024/5/8.
//

#ifndef NET_SOCK_H
#define NET_SOCK_H

#include "net_err.h"
#include <stdint.h>
#include "ipaddr.h"
#include "list.h"
#include "sys_plat.h"

struct sock_t;
struct x_sockaddr;
typedef int x_socklen_t;

typedef struct {
    //close socket
    net_err_t (*close) (struct sock_t * s);
    //send data to socket
    net_err_t (*sendto) (struct sock_t * s, const void * buf, size_t len, int flags,
            const struct x_sockaddr * dest, x_socklen_t dest_len, ssize_t * result_len);
    //recv data from socket
    net_err_t (*recvfrom) (struct sock_t * s, void * buf, size_t len, int flags,
            const struct x_sockaddr * src, x_socklen_t src_len, ssize_t * result_len);
    //set options
    net_err_t (*setopt) (struct sock_t * s, int level, int optname, const char * optval, int optlen);
    //destroy socket
    void (*destroy) (struct sock_t * s);
} sock_ops_t;

typedef struct sock_t {
    uint16_t local_port;
    ipaddr_t local_ip;
    uint16_t remote_port;
    ipaddr_t remote_ip;

    const sock_ops_t * ops;
    int family;
    int protocol;
    int err;
    int rcv_tmo;
    int send_tmo;

    node_t node;
} sock_t;

typedef struct {
    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED
    } state;
    sock_t * sock;
} x_socket_t;

typedef struct {
    int family;
    int type;
    int protocol;
} sock_create_t;

typedef struct {
    const void * buf;
    size_t len;
    int flags;
    const struct x_sockaddr * addr;
    x_socklen_t addr_len;
    ssize_t comp_len;
} sock_data_t;

typedef struct {
    int sockfd;
    union {
        sock_create_t create;
        sock_data_t data;
    };
} sock_req_t;

net_err_t socket_init();

net_err_t sock_init(sock_t * sock, int family, int protocol, const sock_ops_t * ops);

#endif //NET_SOCK_H
