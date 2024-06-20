//
// Created by wj on 2024/5/11.
//

#ifndef NET_UDP_H
#define NET_UDP_H

#include "sock.h"
#include "pktbuf.h"

#define DNS_PORT    53

#pragma pack(1)

typedef struct {
    ipaddr_t from;
    uint16_t port;
} udp_from_t;

typedef struct {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t total_len;
    uint16_t checksum;
} udp_hdr_t;

typedef struct {
    udp_hdr_t hdr;
    uint8_t data[1];
}udp_pkt_t;

#pragma pack()

typedef struct {
    sock_t base;
    list_t recv_list;
    sock_wait_t rcv_wait;
} udp_t;

/**
 * init udp module
 */
net_err_t udp_init();

/**
 * create udp sock
 */
sock_t * udp_create(int family, int protocol);

/**
 * output udp packet
 */
net_err_t udp_out(ipaddr_t * dest, uint16_t dport, ipaddr_t * src, uint16_t sport, pktbuf_t * buf);

/**
 * input udp packet
 */
net_err_t udp_in(pktbuf_t * buf, ipaddr_t * src_ip, ipaddr_t * dest_ip);

/**
 * send udp packet to given dest
 */
net_err_t udp_sendto(struct sock_t * s, const void * buf, size_t len, int flags,
                     const struct x_sockaddr * dest, x_socklen_t dest_len, ssize_t * result_len);

#endif //NET_UDP_H
