//
// Created by wj on 2024/5/14.
//

#ifndef NET_TCP_H
#define NET_TCP_H

#include "net_err.h"
#include "sock.h"
#include "net_cfg.h"
#include "pktbuf.h"

#pragma pack(1)
typedef struct {
    //src port
    uint16_t sport;
    //dest port
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    union {
        uint16_t flag;
#if NET_ENDIAN_LITTLE
    struct {
        uint16_t resv : 4;
        uint16_t shdr : 4;
        uint16_t f_fin : 1;
        uint16_t f_syn : 1;
        uint16_t f_rst : 1;
        uint16_t f_psh : 1;
        uint16_t f_ack : 1;
        uint16_t f_urg : 1;
        uint16_t f_ece : 1;
        uint16_t f_cwr : 1;
    };
#else
    struct {
        //size hdr
        uint16_t shdr : 4;
        uint16_t resv : 4;
        uint16_t f_cwr : 1;
        uint16_t f_ece : 1;
        uint16_t f_urg : 1;
        uint16_t f_ack : 1;
        uint16_t f_psh : 1;
        uint16_t f_rst : 1;
        uint16_t f_syn : 1;
        uint16_t f_fin : 1;
    };
#endif
    };
    uint16_t win;
    uint16_t checksum;
    uint16_t urg_ptr;
} tcp_hdr_t;

typedef struct {
    tcp_hdr_t hdr;
    uint8_t data[1];
} tcp_pkt_t;

#pragma pack()

typedef struct {
    ipaddr_t local_ip;
    ipaddr_t remote_ip;
    tcp_hdr_t * hdr;
    pktbuf_t * buf;
    uint32_t data_len;
    uint32_t seq;
    uint32_t seq_len;
} tcp_seg_t;

typedef struct {
    sock_t base;

    struct {
        uint32_t syn_out: 1;
    } flags;

    struct {
        sock_wait_t wait;
    } conn;

    struct {
        //un ack
        uint32_t una;
        //next
        uint32_t nxt;
        //initial sequence
        uint32_t iss;
        sock_wait_t wait;
    } snd;

    struct {
        uint32_t nxt;
        uint32_t iss;
        sock_wait_t wait;
    }rcv;
} tcp_t;

#if DEBUG_DISP_ENABLED(DEBUG_TCP)
void tcp_show_info(char * msg, tcp_t * tcp);
void tcp_show_pkt(char * msg, tcp_hdr_t * tcp_hdr, pktbuf_t * buf);
void tcp_show_list(void);
#else
#define tcp_show_info(msg, tcp)
#define tcp_show_pkt(msg, tcp_hdr, buf)
#define tcp_show_list()
#endif

net_err_t tcp_init();

/**
 * create tcp
 */
sock_t * tcp_create(int family, int protocol);

/**
 * @return tcp header size
 */
static inline int tcp_hdr_size(tcp_hdr_t * hdr)
{
    return hdr->shdr * 4;
}

/**
 * set tcp header size
 */
static inline void tcp_set_hdr_size(tcp_hdr_t * hdr, int size)
{
    hdr->shdr = size / 4;
}

#endif //NET_TCP_H
