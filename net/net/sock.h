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

#define SOCK_WAIT_READ    (1<<0)
#define SOCK_WAIT_WRITE   (1<<1)
#define SOCK_WAIT_CONN    (1<<2)
#define SOCK_WAIT_ALL     (SOCK_WAIT_READ | SOCK_WAIT_WRITE | SOCK_WAIT_CONN)

typedef struct {
    sys_sem_t sem;
    net_err_t err;
    int waiting;
} sock_wait_t;

typedef struct {
    //close socket
    net_err_t (*close) (struct sock_t * s);
    //send data to socket
    net_err_t (*sendto) (struct sock_t * s, const void * buf, size_t len, int flags,
            const struct x_sockaddr * dest, x_socklen_t dest_len, ssize_t * result_len);
    //send data to socket
    net_err_t (*send) (struct sock_t * s, const void * buf, size_t len, int flags, ssize_t * result_len);
    //recv data from socket
    net_err_t (*recvfrom) (struct sock_t * s, void * buf, size_t len, int flags,
            const struct x_sockaddr * src, x_socklen_t * src_len, ssize_t * result_len);
    //recv data from socket
    net_err_t (*recv) (struct sock_t * s, void * buf, size_t len, int flags, ssize_t * result_len);
    //set options
    net_err_t (*setopt) (struct sock_t * s, int level, int optname, const char * optval, int optlen);
    //destroy socket
    void (*destroy) (struct sock_t * s);
    //connect socket
    net_err_t (*connect)(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len);
    //bind socket
    net_err_t (*bind)(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len);
    //listen
    net_err_t (*listen) (struct sock_t * s, int backlog);
    //accept
    net_err_t (*accept) (struct sock_t * s, struct x_sockaddr * addr, x_socklen_t len, struct sock_t ** client);
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

    sock_wait_t * rcv_wait;
    sock_wait_t * snd_wait;
    sock_wait_t * conn_wait;

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
    x_socklen_t * addr_len;
    ssize_t comp_len;
} sock_data_t;

typedef struct {
    int level;
    int optname;
    const char * optval;
    int len;
} sock_opt_t;

typedef struct {
    const struct x_sockaddr * addr;
    x_socklen_t len;
} sock_conn_t;

typedef struct {
    const struct x_sockaddr * addr;
    x_socklen_t len;
} sock_bind_t;

typedef struct {
    int backlog;
} sock_listen_t;

typedef struct {
    struct x_sockaddr * addr;
    x_socklen_t * len;

    int client;
} sock_accept_t;

typedef struct {
    sock_wait_t * wait;
    int wait_tmo;
    int sockfd;
    union {
        sock_create_t create;
        sock_data_t data;
        sock_opt_t opt;
        sock_conn_t conn;
        sock_bind_t bind;
        sock_listen_t listen;
        sock_accept_t accept;
    };
} sock_req_t;

net_err_t socket_init();

net_err_t sock_init(sock_t * sock, int family, int protocol, const sock_ops_t * ops);

void sock_uninit(sock_t * sock);

void sock_wakeup(sock_t * sock, int type, int err);

net_err_t sock_wait_init(sock_wait_t * wait);

void sock_wait_destroy(sock_wait_t * wait);

void sock_wait_add(sock_wait_t * wait, int tmo, sock_req_t * req);

net_err_t sock_wait_enter(sock_wait_t * wait, int tmo);

void sock_wait_leave(sock_wait_t * wait, net_err_t err);

net_err_t sock_setopt(struct sock_t * s, int level, int optname, const char * optval, int optlen);

net_err_t sock_connect(sock_t * sock, const struct x_sockaddr * addr, x_socklen_t len);

net_err_t sock_send(struct sock_t * s, const void * buf, size_t len, int flags, ssize_t * result_len);

net_err_t sock_recv(struct sock_t * s, void * buf, size_t len, int flags, ssize_t * result_len);

net_err_t sock_bind(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len);

#endif //NET_SOCK_H
