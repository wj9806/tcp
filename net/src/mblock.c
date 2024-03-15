//
// Created by xsy on 2024/3/15.
//

#include "mblock.h"
#include "debug.h"

net_err_t mblock_init(mblock_t * mblock, void * mem, int blk_size, int cnt, locker_type_t type)
{
    uint8_t * buf = (uint8_t *) mem;

    //init cnt memory-blocks
    list_init(&mblock->free_list);
    for (int i = 0; i < cnt; ++i, buf += blk_size) {
        //Set the address of the memory-block to the address of the node
        node_t * node = (node_t *) buf;
        node_init(node);
        list_insert_last(&mblock->free_list, node);
    }

    locker_init(&mblock->locker, type);
    if (type != LOCKER_NONE)
    {
        mblock->alloc_sem = sys_sem_create(cnt);
        if (mblock->alloc_sem == SYS_SEM_INVALID)
        {
            debug_error(DEBUG_MBLOCK, "create sem failed");
            locker_destroy(&mblock->locker);
            return NET_ERR_SYS;
        }
    }
    mblock->start = mem;
    return NET_ERR_OK;
}

void * mblock_alloc(mblock_t * block, int ms)
{
    if ((ms < 0) || (block->locker.type == LOCKER_NONE))
    {
        locker_lock(&block->locker);
        int count = list_count(&block->free_list);
        if (count == 0)
        {
            locker_unlock(&block->locker);
            return (void *) 0;
        }

        node_t * n_block = list_remove_first(&block->free_list);
        locker_unlock(&block->locker);
        return n_block;
    }
    else
    {
        //need wait available memory-block
        if (sys_sem_wait(block->alloc_sem, ms) < 0)
        {
            return (void *) 0;
        }
        else
        {
            locker_lock(&block->locker);
            node_t * n_block = list_remove_first(&block->free_list);
            locker_unlock(&block->locker);
            return n_block;
        }
    }
}

int mblock_free_cnt(mblock_t * block)
{
    locker_lock(&block->locker);
    int count = list_count(&block->free_list);
    locker_unlock(&block->locker);
    return count;
}

void mblock_free(mblock_t * mblock, void * block)
{
    locker_lock(&mblock->locker);
    //free memory-block to free_list
    list_insert_last(&mblock->free_list, (node_t *) block);
    locker_unlock(&mblock->locker);

    if (mblock->locker.type != LOCKER_NONE)
    {
        //notify consumer threads
        sys_sem_notify(mblock->alloc_sem);
    }
}

void mblock_destroy(mblock_t * mblock)
{
    if (mblock->locker.type != LOCKER_NONE)
    {
        sys_sem_free(mblock->alloc_sem);
        locker_destroy(&mblock->locker);
    }
}
