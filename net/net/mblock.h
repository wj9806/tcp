//
// Created by xsy on 2024/3/15.
//

#ifndef NET_MBLOCK_H
#define NET_MBLOCK_H

#include "list.h"
#include "sys_plat.h"
#include "locker.h"

/**
 * memory-block
 */
typedef struct mblock_t
{
    //free memory-block linked list
    list_t free_list;
    //the addr start of memory-block array
    void * start;
    //mblock locker
    locker_t locker;
    //alloc memory-block sem
    sys_sem_t alloc_sem;
} mblock_t;

net_err_t mblock_init(mblock_t * mblock, void * mem, int blk_size, int cnt, locker_type_t type);

/**
 * alloc a memory-block
 * @param ms Timeout period, in milliseconds
 * @return memory-block addr
 */
void * mblock_alloc(mblock_t * block, int ms);

/**
 * @return free memory-block count
 */
int mblock_free_cnt(mblock_t * block);

/**
 * @param block memory-block that need to be freed
 */
void mblock_free(mblock_t * mblock, void * block);

/**
 * destroy the mblock
 */
void mblock_destroy(mblock_t * mblock);

#endif //NET_MBLOCK_H
