//
// Created by wj on 2024/3/18.
//

#ifndef NET_FIXQ_H
#define NET_FIXQ_H

#include "locker.h"
#include "sys_plat.h"

//fixed size queue
typedef struct fixq_t {
    int size;
    int in, out, cnt;
    void ** buf;

    locker_t  locker;

    sys_sem_t recv_sem;
    sys_sem_t send_sem;
} fixq_t;

net_err_t fixq_init(fixq_t * q, void ** buf, int size, locker_type_t type);

/**
 * send message
 * @param q fixed size queue
 * @param msg message
 * @param ms wait time
 */
net_err_t fixq_send(fixq_t * q, void * msg, int ms);

/**
 * recv message
 */
void * fixq_recv(fixq_t * q, int ms);

/**
 * destroy the queue
 */
void fixq_destroy(fixq_t * q);

int fixq_count(fixq_t * q);

#endif //NET_FIXQ_H
