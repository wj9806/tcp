//
// Created by xsy on 2024/3/15.
//

#ifndef NET_MBLOCK_H
#define NET_MBLOCK_H

#include "list.h"
#include "sys_plat.h"
#include "locker.h"

typedef struct _mblock_t
{
    list_t free_list;
    void * start;
    locker_t locker;
    sys_sem_t alloc_sem;
} mblock_t;

#endif //NET_MBLOCK_H
