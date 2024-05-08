//
// Created by wj on 2024/5/7.
//

#include "socket.h"
#include "exmsg.h"
#include "sock.h"

//create socket
net_err_t sock_create_req_in(struct func_msg_t * msg);

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
