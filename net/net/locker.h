//
// Created by xsy on 2024/3/15.
//

#ifndef NET_NLOCKER_H
#define NET_NLOCKER_H

#include "sys_plat.h"
#include "net_err.h"

typedef enum _locker_type_t{
    LOCKER_NONE,
    LOCKER_THREAD,
} locker_type_t;

typedef struct _locker_t
{
    locker_type_t type;
    union {
        sys_mutex_t mutex;
    };

} locker_t;

//locker init function
net_err_t locker_init(locker_t * locker, locker_type_t type);

void locker_destroy(locker_t * locker);

void locker_lock(locker_t * locker);

void locker_unlock(locker_t * locker);

#endif //NET_NLOCKER_H
