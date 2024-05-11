//
// Created by wj on 2024/5/8.
//
#include "sock.h"
#include "sys_plat.h"
#include "exmsg.h"
#include "raw.h"
#include "socket.h"
#include "udp.h"

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
    sock->rcv_wait = (sock_wait_t *)0;
    sock->snd_wait = (sock_wait_t *)0;
    sock->conn_wait = (sock_wait_t *)0;
    node_init(&sock->node);
    return NET_ERR_OK;
}

void sock_uninit(sock_t * sock)
{
    if (sock->rcv_wait)
    {
        sock_wait_destroy(sock->rcv_wait);
    }

    if (sock->snd_wait)
    {
        sock_wait_destroy(sock->snd_wait);
    }

    if (sock->conn_wait)
    {
        sock_wait_destroy(sock->conn_wait);
    }
}

//wakeup waited thread
void sock_wakeup(sock_t * sock, int type, int err)
{
    if (type & SOCK_WAIT_CONN)
    {
        sock_wait_leave(sock->conn_wait, err);
    }

    if (type & SOCK_WAIT_WRITE)
    {
        sock_wait_leave(sock->snd_wait, err);
    }

    if (type & SOCK_WAIT_READ)
    {
        sock_wait_leave(sock->rcv_wait, err);
    }
}

net_err_t sock_wait_init(sock_wait_t * wait)
{
    wait->waiting = 0;
    wait->err = NET_ERR_OK;
    wait->sem = sys_sem_create(0);
    return wait->sem == SYS_SEM_INVALID ? NET_ERR_SYS : NET_ERR_OK;
}

void sock_wait_destroy(sock_wait_t * wait)
{
    if (wait->sem != SYS_SEM_INVALID)
        sys_sem_free(wait->sem);
}

void sock_wait_add(sock_wait_t * wait, int tmo, sock_req_t * req)
{
    wait->waiting++;
    req->wait = wait;
    req->wait_tmo = tmo;
}

net_err_t sock_wait_enter(sock_wait_t * wait, int tmo)
{
    if (sys_sem_wait(wait->sem, tmo) < 0)
    {
        return NET_ERR_TMO;
    }

    return wait->err;
}

void sock_wait_leave(sock_wait_t * wait, net_err_t err)
{
    if (wait->waiting > 0)
    {
        wait->waiting--;
        wait->err = err;
        sys_sem_notify(wait->sem);
    }
}

net_err_t sock_create_req_in(struct func_msg_t * msg)
{
    static const struct socket_info_t {
        int protocol;
        sock_t * (*create) (int family, int protocol);
    } sock_tbl[] = {
            [SOCK_RAW] = {.protocol = IPPROTO_ICMP, .create = raw_create,},
            [SOCK_DGRAM] = {.protocol = IPPROTO_UDP, .create = udp_create,},
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
    net_err_t err = sock->ops->sendto(sock, data->buf, data->len, data->flags, data->addr, *data->addr_len, &data->comp_len);

    if (err == NET_ERR_NEED_WAIT)
    {
        if (sock->snd_wait)
        {
            sock_wait_add(sock->snd_wait, sock->send_tmo, req);
        }
    }

    return err;
}

net_err_t sock_recvfrom_req_in(struct func_msg_t * msg)
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
    if (!sock->ops->recvfrom)
    {
        debug_error(DEBUG_SOCKET, "recvfrom func no impl");
        return NET_ERR_NONE;
    }

    net_err_t err = sock->ops->recvfrom(sock, (void *)data->buf, data->len, data->flags, data->addr, data->addr_len, &data->comp_len);
    if (err == NET_ERR_NEED_WAIT)
    {
        if (sock->rcv_wait)
        {
            sock_wait_add(sock->rcv_wait, sock->rcv_tmo, req);
        }
    }

    return err;
}

net_err_t sock_setopt(struct sock_t * s, int level, int optname, const char * optval, int optlen)
{
    if (level != SOL_SOCKET)
    {
        debug_error(DEBUG_SOCKET, "unknown level");
        return NET_ERR_PARAM;
    }

    switch (optname) {
        case SO_RCVTIMEO:
        case SO_SNDTIMEO:
            if (optlen != sizeof(struct x_timeval))
            {
                debug_error(DEBUG_SOCKET, "time size error");
                return NET_ERR_PARAM;
            }

            struct x_timeval* time = (struct x_timeval*) optval;
            int time_ms = time->tv_sec * 1000 + time->tv_usec / 1000;
            if (optname == SO_RCVTIMEO)
            {
                s->rcv_tmo = time_ms;
                return NET_ERR_OK;
            }
            else if (optname == SO_SNDTIMEO)
            {
                s->send_tmo = time_ms;
                return NET_ERR_OK;
            }
            break;
        default:
            break;
    }
}

net_err_t sock_setsockopt_req_in(struct func_msg_t * msg)
{
    sock_req_t * req = (sock_req_t *)msg->param;

    x_socket_t * s = get_socket(req->sockfd);
    if (!s)
    {
        debug_error(DEBUG_SOCKET, "param error");
        return NET_ERR_PARAM;
    }

    sock_t * sock = s->sock;
    sock_opt_t * opt = &req->opt;

    return sock->ops->setopt(sock, opt->level, opt->optname, opt->optval, opt->len);
}

net_err_t sock_close_req_in(struct func_msg_t * msg)
{
    sock_req_t * req = (sock_req_t *)msg->param;

    x_socket_t * s = get_socket(req->sockfd);
    if (!s)
    {
        debug_error(DEBUG_SOCKET, "param error");
        return NET_ERR_PARAM;
    }

    sock_t * sock = s->sock;
    if (!sock->ops->close)
    {
        debug_error(DEBUG_SOCKET, "close func no impl");
        return NET_ERR_NONE;
    }
    net_err_t err = sock->ops->close(sock);
    socket_free(s);
    return err;
}