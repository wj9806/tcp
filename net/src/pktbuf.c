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

/**
 * free pktblk_t
 */
static void pktblk_free_list(pktblk_t * first)
{
    while (first)
    {
        pktblk_t * next = pktblk_blk_next(first);
        mblock_free(&block_list, first);
        first = next;
    }
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

/**
 * insert pktblk_t into pktbuf_t
 * @param front if front equals 1, then insert into tail
 */
static void pktbuf_insert_blk_list(pktbuf_t * buf, pktblk_t * block, int last)
{
    if (last)
    {
        while (block)
        {
            pktblk_t * next_blk = pktblk_blk_next(block);
            list_insert_last(&buf->blk_list, &block->node);
            buf->total_size += block->size;
            block = next_blk;
        }
    }
    else
    {
        pktblk_t * pre = (pktblk_t *) 0;
        while (block)
        {
            pktblk_t * next_blk = pktblk_blk_next(block);
            if (pre) {
                list_insert_after(&buf->blk_list, &pre->node, &block->node);
            }
            else
            {
                list_insert_first(&buf->blk_list, &block->node);
            }
            buf->total_size += block->size;
            pre = block;
            block = next_blk;
        }
    }

}

pktbuf_t * pktbuf_alloc(int size)
{
    pktbuf_t * buf = mblock_alloc(&pktbuf_list, -1);
    if (!buf) {
        debug_error(DEBUG_PKTBUF, "no buffer");
        return (pktbuf_t *)0;
    }
    buf->total_size = 0;
    list_init(&buf->blk_list);
    node_init(&buf->node);

    if(size) {
        pktblk_t * block = pktblk_alloc_list(size, 1);
        if(!block) {
            //alloc failed
            mblock_free(&pktbuf_list, buf);
            return (pktbuf_t *)0;
        }
        pktbuf_insert_blk_list(buf, block, 1);
    }
    display_check_buf(buf);
    return buf;
}

void pktbuf_free(pktbuf_t * buf)
{
    pktblk_free_list(pktbuf_first_blk(buf));
    mblock_free(&pktbuf_list, buf);
}


net_err_t pktbuf_add_header(pktbuf_t * buf, int size, int cont)
{
    pktblk_t * block = pktbuf_first_blk(buf);
    int resv_size = (int)(block->data - block->payload);
    if(size <= resv_size)
    {
        block->size += size;
        block->data -= size;
        buf->total_size += size;
        display_check_buf(buf);
        return NET_ERR_OK;
    }
    if (cont)
    {
        if(size > PKTBUF_BLK_SIZE)
        {
            debug_error(DEBUG_PKTBUF, "set cont, size to big: %d > %d", size, PKTBUF_BLK_SIZE);
            return NET_ERR_SIZE;
        }
        block = pktblk_alloc_list(size, 1);
        if(!block)
        {
            debug_error(DEBUG_PKTBUF, "no buffer %d", size);
            return NET_ERR_NONE;
        }
    }
    else
    {

    }

    pktbuf_insert_blk_list(buf, block, 0);
    display_check_buf(buf);
    return NET_ERR_OK;
}