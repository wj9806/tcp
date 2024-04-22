//
// Created by wj on 2024/4/9.
//

#ifndef NET_PROTOCOL_H
#define NET_PROTOCOL_H

typedef enum {
    NET_PROTOCOL_ARP = 0x0806,
    NET_PROTOCOL_IPV4 = 0x0800,
    NET_PROTOCOL_ICMPv4 = 1,
    NET_PROTOCOL_UDP = 0x11,
    NET_PROTOCOL_TCP = 0x6,

} protocol_t;

#endif //NET_PROTOCOL_H
