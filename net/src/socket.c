//
// Created by wj on 2024/5/7.
//

#include "socket.h"
#include "exmsg.h"

//create socket
net_err_t sock_create_req_in(struct func_msg_t * msg);

//send data
net_err_t sock_sendto_req_in(struct func_msg_t * msg);

int x_socket(int family, int type, int protocol)
{
    sock_req_t req;
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
        req.data.buf = start;
        req.data.len = len;
        req.data.flags = flags;
        req.data.addr = dest;
        req.data.addr_len = dest_len;
        req.data.comp_len = 0;
        net_err_t err = exmsg_func_exec(sock_sendto_req_in, &req);
        if (err < 0)
        {
            debug_info(DEBUG_SOCKET, "create socket failed");
            return -1;
        }

        len -= req.data.comp_len;
        start += req.data.comp_len;
        send_size += req.data.comp_len;
    }

    return send_size;
}
