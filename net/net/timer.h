//
// Created by wj on 2024/4/10.
//

#ifndef NET_TIMER_H
#define NET_TIMER_H

#include "net_cfg.h"
#include "net_err.h"
#include "list.h"

#define NET_TIMER_RELOAD        (1 << 0)

struct net_timer_t;

typedef void (*timer_proc_t)(struct net_timer_t * timer, void * arg);

//timer
typedef struct net_timer_t {
    char name[TIMER_NAME_SIZE];
    int flags;
    int curr;
    int reload;
    //executed function
    timer_proc_t proc;
    //function args
    void * arg;
    node_t node;
} net_timer_t;

net_err_t net_timer_init(void);

/**
 * add a timer
 * @param timer timer
 * @param name timer's name
 * @param proc timer's executed function
 * @param arg timer's executed function's args
 * @param ms delay timer
 * @param flags is or not reload
 * @return
 */
net_err_t net_timer_add(net_timer_t * timer, const char * name, timer_proc_t proc, void * arg, int ms, int flags);

/**
 * remove the given timer
 */
void net_timer_remove(net_timer_t * timer);

#endif //NET_TIMER_H
