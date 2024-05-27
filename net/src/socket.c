//
// Created by wj on 2024/5/7.
//

#include "socket.h"
#include "exmsg.h"

//create socket
net_err_t sock_create_req_in(struct func_msg_t * msg);

//send data
net_err_t sock_sendto_req_in(struct func_msg_t * msg);

//send data
net_err_t sock_send_req_in(struct func_msg_t * msg);

//recv data
net_err_t sock_recvfrom_req_in(struct func_msg_t * msg);

//recv data
net_err_t sock_recv_req_in(struct func_msg_t * msg);

//set sockopt
net_err_t sock_setsockopt_req_in(struct func_msg_t * msg);

//close socket
net_err_t sock_close_req_in(struct func_msg_t * msg);

//connect socket
net_err_t sock_connect_req_in(struct func_msg_t * msg);

//bind socket
net_err_t sock_bind_req_in(struct func_msg_t * msg);

int x_socket(int family, int type, int protocol)
{
    sock_req_t req;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    req.sockfd = -1;
    req.create.family = family;
    req.create.type = type;
    req.create.protocol = protocol;
    net_err_t err = exmsg_func_exec(sock_create_req_in, &req);
    if (err < 0)
    {
        debug_info(DEBUG_SOCKET, "create socket failed");
        return -1;
    }
    return req.sockfd;
}

ssize_t x_sendto(int s, const void * buf, size_t len, int flags, const struct x_sockaddr * dest, x_socklen_t dest_len)
{
    if (!buf || !len)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return -1;
    }

    if (dest->sin_family != AF_INET || dest_len != sizeof(struct x_sockaddr_in))
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return -1;
    }

    uint8_t * start = (uint8_t *)buf;
    ssize_t send_size = 0;
    while (len > 0)
    {
        sock_req_t req;
        req.sockfd = s;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.addr = dest;
        req.data.addr_len = &dest_len;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_sendto_req_in, &req);
        if (err < 0)
        {
            debug_info(DEBUG_SOCKET, "create socket failed");
            return -1;
        }

        if (req.wait)
        {
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err < 0)
            {
                debug_error(DEBUG_SOCKET, "send failed");
                return err;
            }
        }
        len -= req.data.comp_len;
        start += req.data.comp_len;
        send_size += req.data.comp_len;
    }

    return send_size;
}

ssize_t x_send(int s, const void * buf, size_t len, int flags)
{
    if (!buf || !len)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return -1;
    }

    uint8_t * start = (uint8_t *)buf;
    ssize_t send_size = 0;
    while (len > 0)
    {
        sock_req_t req;
        req.sockfd = s;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_send_req_in, &req);
        if (err < 0)
        {
            debug_info(DEBUG_SOCKET, "create socket failed");
            return -1;
        }

        if (req.wait)
        {
            err = sock_wait_enter(req.wait, req.wait_tmo);
            if (err < 0)
            {
                debug_error(DEBUG_SOCKET, "send failed");
                return err;
            }
        }
        len -= req.data.comp_len;
        start += req.data.comp_len;
        send_size += req.data.comp_len;
    }

    return send_size;
}

ssize_t x_recvfrom(int s, const void * buf, size_t len, int flags, const struct x_sockaddr * src, x_socklen_t * src_len)
{
    if (!buf || !len || !src)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return NET_ERR_PARAM;
    }

    while (1)
    {
        sock_req_t req;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;
        req.sockfd = s;
        req.data.buf = buf;
        req.data.len = len;
        req.data.flags = flags;
        req.data.addr = src;
        req.data.addr_len = src_len;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_recvfrom_req_in, &req);
        if (err < 0)
        {
            debug_info(DEBUG_SOCKET, "recv socket failed");
            return -1;
        }

        if (req.data.comp_len)
        {
            return (ssize_t)req.data.comp_len;
        }

        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0)
        {
            debug_error(DEBUG_SOCKET, "recv failed");
            return err;
        }
    }
}

ssize_t x_recv(int s, const void * buf, size_t len, int flags)
{
    if (!buf || !len)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return NET_ERR_PARAM;
    }

    while (1)
    {
        sock_req_t req;
        req.wait = (sock_wait_t *)0;
        req.wait_tmo = 0;
        req.sockfd = s;
        req.data.buf = buf;
        req.data.len = len;
        req.data.flags = flags;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_recv_req_in, &req);
        if (err < 0)
        {
            debug_info(DEBUG_SOCKET, "recv socket failed");
            return -1;
        }

        if (req.data.comp_len)
        {
            return (ssize_t)req.data.comp_len;
        }

        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0)
        {
            debug_error(DEBUG_SOCKET, "recv failed");
            return err;
        }
    }
}

int x_setsockopt(int s, int level, int optname, const char * optval, int len)
{
    if (!optval || !len)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return NET_ERR_PARAM;
    }

    sock_req_t req;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    req.sockfd = s;
    req.opt.level = level;
    req.opt.optname = optname;
    req.opt.optval = optval;
    req.opt.len = len;
    net_err_t err = exmsg_func_exec(sock_setsockopt_req_in, &req);
    if (err < 0)
    {
        debug_info(DEBUG_SOCKET, "set sock opt failed");
        return -1;
    }
    return 0;
}

int x_close(int s)
{
    sock_req_t req;
    req.wait = (sock_wait_t *)0;
    req.wait_tmo = 0;
    req.sockfd = s;
    net_err_t err = exmsg_func_exec(sock_close_req_in, &req);
    if (err < 0)
    {
        debug_info(DEBUG_SOCKET, "set sock opt failed");
        return -1;
    }

    if (req.wait)
    {
        sock_wait_enter(req.wait, req.wait_tmo);
    }
    //TODO: close resource
    return 0;
}

int x_connect(int s, const struct x_sockaddr * addr, x_socklen_t len)
{
    if (!addr || (len != sizeof(struct x_sockaddr)) || s < 0)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return -1;
    }

    if (addr->sin_family != AF_INET)
    {
        debug_error(DEBUG_SOCKET, "invalid sin_family");
        return -1;
    }

    sock_req_t req;
    req.wait = 0;
    req.sockfd = s;
    req.conn.addr = addr;
    req.conn.len = len;

    net_err_t err = exmsg_func_exec(sock_connect_req_in, &req);
    if (err < 0)
    {
        debug_info(DEBUG_SOCKET, "connect sock failed");
        return -1;
    }

    if (req.wait)
    {
        err = sock_wait_enter(req.wait, req.wait_tmo);
        if (err < 0)
        {
            debug_error(DEBUG_SOCKET, "send failed");
            return err;
        }
    }
    return 0;
}

int x_bind(int s, const struct x_sockaddr * addr, x_socklen_t len)
{
    if (!addr || (len != sizeof(struct x_sockaddr)) || s < 0)
    {
        debug_error(DEBUG_SOCKET, "invalid param");
        return -1;
    }

    if (addr->sin_family != AF_INET)
    {
        debug_error(DEBUG_SOCKET, "invalid sin_family");
        return -1;
    }

    sock_req_t req;
    req.wait = 0;
    req.sockfd = s;
    req.bind.addr = addr;
    req.bind.len = len;

    net_err_t err = exmsg_func_exec(sock_bind_req_in, &req);
    if (err < 0)
    {
        debug_info(DEBUG_SOCKET, "bind sock failed");
        return -1;
    }

    return 0;
}