//
// Created by wj on 2024/5/8.
//
#include "sock.h"
#include "sys_plat.h"
#include "exmsg.h"
#include "raw.h"
#include "socket.h"

#define SOCKET_MAX_NR   (RAW_MAX_NR)

static x_socket_t socket_tbl[SOCKET_MAX_NR];

/**
 * get the index of the socket in socket_tbl
 */
static int get_index(x_socket_t * s)
{
    return (int)(s - socket_tbl);
}

/**
 * get the socket at the specified index
 */
static x_socket_t * get_socket(int index)
{
    if (index < 0 || index >= SOCKET_MAX_NR)
        return (x_socket_t *)0;
    return socket_tbl + index;
}

/**
 * alloc a socket
 */
static x_socket_t * socket_alloc()
{
    x_socket_t * s = (x_socket_t*)0;
    for (int i = 0; i < SOCKET_MAX_NR; ++i) {
        x_socket_t * curr = socket_tbl + i;
        if (curr->state == SOCKET_STATE_FREE)
        {
            curr->state = SOCKET_STATE_USED;
            s = curr;
            break;
        }
    }
    return s;
}

/**
 * free socket
 */
static void socket_free(x_socket_t * s)
{
    s->state = SOCKET_STATE_FREE;
}


net_err_t socket_init()
{
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));
    return NET_ERR_OK;
}

net_err_t sock_init(sock_t * sock, int family, int protocol, const sock_ops_t * ops)
{
    sock->protocol = protocol;
    sock->family = family;
    sock->ops = ops;

    ipaddr_set_any(&sock->local_ip);
    ipaddr_set_any(&sock->remote_ip);
    sock->local_port = 0;
    sock->remote_port = 0;
    sock->err = NET_ERR_OK;
    sock->rcv_tmo = 0;
    sock->send_tmo = 0;
    node_init(&sock->node);
    return NET_ERR_OK;
}

net_err_t sock_create_req_in(struct func_msg_t * msg)
{
    static const struct socket_info_t {
        int protocol;
        sock_t * (*create) (int family, int protocol);
    } sock_tbl[] = {
            [SOCK_RAW] = {.protocol = IPPROTO_ICMP, .create = raw_create,}
    };

    sock_req_t * req = (sock_req_t *)msg->param;
    sock_create_t * param = &req->create;
    x_socket_t * s = socket_alloc();
    if (!s)
    {
        debug_error(DEBUG_SOCKET, "no socket");
        return NET_ERR_MEM;
    }
    if (param->type < 0 || param->type >= (sizeof(sock_tbl) / sizeof(sock_tbl[0])))
    {
        debug_error(DEBUG_SOCKET, "param type error: %d", param->type);
        return NET_ERR_PARAM;
    }

    const struct socket_info_t * info = sock_tbl + param->type;
    if (param->protocol == 0)
    {
        param->protocol = info->protocol;
    }
    //create sock
    sock_t * sock = info->create(param->family, param->protocol);
    if (!sock)
    {
        debug_error(DEBUG_SOCKET, "create sock failed");
        socket_free(s);
        return NET_ERR_MEM;
    }
    s->sock = sock;
    req->sockfd = get_index(s);
    return NET_ERR_OK;
}

net_err_t sock_sendto_req_in(struct func_msg_t * msg)
{
    sock_req_t * req = (sock_req_t *)msg->param;

    x_socket_t * s = get_socket(req->sockfd);
    if (!s)
    {
        debug_error(DEBUG_SOCKET, "param error");
        return NET_ERR_PARAM;
    }

    sock_t * sock = s->sock;
    sock_data_t * data = &req->data;

    if (!sock->ops->sendto)
    {
        debug_error(DEBUG_SOCKET, "sendto func no impl");
        return NET_ERR_NONE;
    }
    net_err_t err = sock->ops->sendto(sock, data->buf, data->len, data->flags, data->addr, data->addr_len, &data->comp_len);

    return NET_ERR_OK;
}