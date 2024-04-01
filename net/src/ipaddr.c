//
// Created by wj on 2024/3/29.
//
#include "ipaddr.h"

void ipaddr_set_any(ipaddr_t * ip)
{
    ip->type = IPADDR_V4;
    ip->q_addr = 0;
}

net_err_t ipaddr_from_str(ipaddr_t * dest, const char * str)
{
    if (!dest || !str)
    {
        return NET_ERR_PARAM;
    }

    dest->type = IPADDR_V4;
    dest->q_addr = 0;

    uint8_t * p = dest->addr;
    uint8_t sub_addr = 0;
    char c;
    while ((c = *str++) != '\0')
    {
        if ((c >= '0') && (c <= '9'))
        {
            sub_addr = sub_addr * 10 + (c - '0');
        }
        else if (c == '.')
        {
            *p++ = sub_addr;
            sub_addr = 0;
        }
        else
        {
            return NET_ERR_PARAM;
        }
    }
    *p = sub_addr;
    return NET_ERR_OK;
}