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

void ipaddr_to_buf(const ipaddr_t * src, uint8_t * in_buf)
{
    *(uint32_t*)in_buf = src->q_addr;
}

void ipaddr_from_buf(ipaddr_t * dest, const uint8_t * ip_buf)
{
    dest->type = IPADDR_V4;
    dest->q_addr = *(uint32_t *) ip_buf;
}

int ipaddr_is_local_broadcast(const ipaddr_t * ipaddr)
{
    return ipaddr->q_addr == 0xFFFFFFFF;
}

ipaddr_t ipaddr_get_host(const ipaddr_t * ipaddr, const ipaddr_t * netmask)
{
    ipaddr_t host;
    host.q_addr = ipaddr->q_addr & ~netmask->q_addr;
    return host;
}

int ipaddr_is_direct_broadcast(const ipaddr_t * ipaddr, const ipaddr_t * netmask)
{
    ipaddr_t hostid = ipaddr_get_host(ipaddr, netmask);
    return hostid.q_addr == (0xFFFFFFFF & ~netmask->q_addr);
}
