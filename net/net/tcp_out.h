//
// Created by wj on 2024/5/16.
//

#ifndef NET_TCP_OUT_H
#define NET_TCP_OUT_H

#include "tcp.h"

/** send tcp reset packet */
net_err_t tcp_send_reset(tcp_seg_t * seg);

/** send tcp syn packet */
net_err_t tcp_send_syn(tcp_t * tcp);

/** process tcp ack*/
net_err_t tcp_ack_process(tcp_t * tcp, tcp_seg_t * seg);

#endif //NET_TCP_OUT_H
