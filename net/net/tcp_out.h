//
// Created by wj on 2024/5/16.
//

#ifndef NET_TCP_OUT_H
#define NET_TCP_OUT_H

#include "tcp.h"

typedef enum {
    TCP_OEVENT_SEND,
    TCP_OEVENT_XMIT,
    TCP_OEVENT_TMO,
} tcp_oevent_t;

/** send tcp reset packet */
net_err_t tcp_send_reset(tcp_seg_t * seg);

/** send tcp rst packet for given tcp */
net_err_t tcp_send_reset_for_tcp(tcp_t * tcp);

/** send tcp keepalive packet */
net_err_t tcp_send_keepalive(tcp_t * tcp);

/** send tcp syn packet */
net_err_t tcp_send_syn(tcp_t * tcp);

/** process tcp ack*/
net_err_t tcp_ack_process(tcp_t * tcp, tcp_seg_t * seg);

/** send ack */
net_err_t tcp_send_ack(tcp_t * tcp, tcp_seg_t * seg);

/** send fin*/
net_err_t tcp_send_fin(tcp_t * tcp);

/** write snd buf*/
int tcp_write_sndbuf(tcp_t* tcp, const uint8_t* buf, int len);

/** transmit tcp data packet*/
net_err_t tcp_transmit(tcp_t * tcp);

/** get tcp_ostate_name */
const char * tcp_ostate_name(tcp_t * tcp);

/** output event */
void tcp_out_event(tcp_t * tcp, tcp_oevent_t event);

/** set tcp ostate */
void tcp_set_ostate(tcp_t * tcp, tcp_ostate_t state);

#endif //NET_TCP_OUT_H
