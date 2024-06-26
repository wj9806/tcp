//
// Created by wj on 2024/3/13.
//

#ifndef NET_NET_ERR_H
#define NET_NET_ERR_H

typedef enum net_err_t
{
    NET_ERR_NEED_WAIT = 1,
    NET_ERR_OK = 0,
    NET_ERR_SYS = -1,
    NET_ERR_MEM = -2,
    NET_ERR_FULL = -3,
    NET_ERR_TMO = -4,
    NET_ERR_SIZE = -5,
    NET_ERR_NONE = -6,
    NET_ERR_PARAM = -7,
    NET_ERR_STATE = -8,
    NET_ERR_IO = -9,
    NET_ERR_EXIST = -10,
    NET_ERR_NOT_SUPPORT = -11,
    NET_ERR_BROKEN = -13,
    NET_ERR_UNREACHABLE = -14,
    NET_ERR_BOUND = -14,
    NET_ERR_RESET = -15,
    NET_ERR_CLOSE = -16,
    NET_ERR_UNKNOWN = -17,
} net_err_t;

#endif //NET_NET_ERR_H
