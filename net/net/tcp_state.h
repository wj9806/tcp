//
// Created by wj on 2024/5/23.
//

#ifndef NET_TCP_STATE_H
#define NET_TCP_STATE_H

#include "tcp.h"

//get tcp_state_t string name
const char * tcp_state_name(tcp_state_t state);

//set tcp state
void tcp_set_state(tcp_t * tcp, tcp_state_t state);

#endif //NET_TCP_STATE_H
