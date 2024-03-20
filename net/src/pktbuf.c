//
// Created by wj on 2024/3/20.
//

#include "pktbuf.h"
#include "debug.h"
#include "mblock.h"
#include "locker.h"

static locker_t locker;
static pktblk_t block_buffer[PKTBUF_BLK_CNT];
static mblock_t block_list;
static pktbuf_t pktbuf_buffer[PKTBUF_BUF_CNT];
static mblock_t pktbuf_list;

net_err_t pktbuf_init(void)
{
    debug_info(DEBUG_PKTBUF, "init pktbuf");
    locker_init(&locker, LOCKER_THREAD);

    mblock_init(&block_list, block_buffer, sizeof(pktblk_t), PKTBUF_BLK_CNT, LOCKER_THREAD);
    mblock_init(&pktbuf_list, pktbuf_buffer, sizeof(pktbuf_t), PKTBUF_BUF_CNT, LOCKER_THREAD);
    return NET_ERR_OK;
}

static pktblk_t *pktblk_alloc() {
    pktblk_t * block = mblock_alloc(&block_list, -1);
    if (block)
    {
        block->size = 0;
        block->data = (uint8_t *)0;
        node_init(&block->node);
    }
    return block;
}

static pktblk_t * pktblk_alloc_list(int size, int add_front)
{
    pktblk_t * first_block = (pktblk_t *)0;
    pktblk_t * pre_block = (pktblk_t *)0;
    while (size)
    {
        pktblk_t * new_block = pktblk_alloc();
        if (!new_block)
        {
            debug_error(DEBUG_PKTBUF, "no buffer for alloc(%d)", size);
            return (pktblk_t *)0;
        }
        int curr_size = 0;
        if (add_front)
        {
            //head insert
            curr_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            new_block->size = curr_size;
            new_block->data = new_block->payload + PKTBUF_BLK_SIZE - curr_size;
            if (first_block)
            {
                list_node_set_next(&new_block->node, &first_block->node);
            }
            first_block = new_block;
        }
        else
        {
            //tail insert
            if (!first_block)
            {
                first_block = new_block;
            }
            curr_size = size > PKTBUF_BLK_SIZE ? PKTBUF_BLK_SIZE : size;
            new_block->size = curr_size;
            new_block->data = new_block->payload;
            if (pre_block)
            {
                list_node_set_next(&pre_block->node, &new_block->node);
            }
            pre_block = new_block;
        }
        size -= curr_size;
    }
    return first_block;
}

pktbuf_t * pktbuf_alloc(int size)
{
    //tail insert
    pktblk_alloc_list(size, 0);
    pktblk_alloc_list(size, 1);
    return (pktbuf_t *)0;
}

void pktbuf_free(pktbuf_t * buf)
{

}