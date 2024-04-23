//
// Created by wj on 2024/3/20.
//

#include "pktbuf.h"
#include "debug.h"
#include "mblock.h"
#include "locker.h"
#include "tools.h"

static locker_t locker;

static pktblk_t block_buffer[PKTBUF_BLK_CNT];
static mblock_t block_list;

static pktbuf_t pktbuf_buffer[PKTBUF_BUF_CNT];
static mblock_t pktbuf_list;

net_err_t pktbuf_init(void)
{
    debug_info(DEBUG_PKTBUF, "init pktbuf");
    locker_init(&locker, LOCKER_THREAD);

    mblock_init(&block_list, block_buffer, sizeof(pktblk_t), PKTBUF_BLK_CNT, LOCKER_NONE);
    mblock_init(&pktbuf_list, pktbuf_buffer, sizeof(pktbuf_t), PKTBUF_BUF_CNT, LOCKER_NONE);
    return NET_ERR_OK;
}

static pktblk_t *pktblk_alloc() {
    locker_lock(&locker);
    pktblk_t * block = mblock_alloc(&block_list, -1);
    locker_unlock(&locker);
    if (block)
    {
        block->size = 0;
        block->data = (uint8_t *)0;
        node_init(&block->node);
    }
    return block;
}

static void pktblk_free(pktblk_t * block)
{
    locker_lock(&locker);
    mblock_free(&block_list, block);
    locker_unlock(&locker);
}

/**
 * free pktblk_t
 */
static void pktblk_free_list(pktblk_t * first)
{
    while (first)
    {
        pktblk_t * next = pktblk_blk_next(first);
        pktblk_free(first);
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
            if (first_block)
            {
                pktblk_free_list(first_block);
            }
            return (pktblk_t *)0;
        }
        int curr_size;
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

/**
 * Obtain the remaining data block size of buf
 * @return size
 */
static inline int total_blk_remain(pktbuf_t * buf)
{
    return buf->total_size - buf->pos;
}

/**
 * Obtain the remaining current data block size of buf
 * @return size
 */
static inline int curr_blk_remain(pktbuf_t * buf)
{
    pktblk_t * block = buf->curr_blk;
    if (!block)
    {
        return 0;
    }
    return (int)(block->data + block->size - buf->blk_offset);
}

pktbuf_t * pktbuf_alloc(int size)
{
    locker_lock(&locker);
    pktbuf_t * buf = mblock_alloc(&pktbuf_list, -1);
    locker_unlock(&locker);
    if (!buf) {
        debug_error(DEBUG_PKTBUF, "no buffer");
        return (pktbuf_t *)0;
    }
    buf->total_size = 0;
    buf->ref = 1;
    list_init(&buf->blk_list);
    node_init(&buf->node);

    if(size) {
        pktblk_t * block = pktblk_alloc_list(size, 1);
        if(!block) {
            locker_lock(&locker);
            mblock_free(&pktbuf_list, buf);
            locker_unlock(&locker);
            return (pktbuf_t *)0;
        }
        pktbuf_insert_blk_list(buf, block, 1);
    }
    pktbuf_reset_access(buf);
    display_check_buf(buf);
    return buf;
}

void pktbuf_free(pktbuf_t * buf)
{
    locker_lock(&locker);
    if (--buf->ref == 0)
    {
        pktblk_free_list(pktbuf_first_blk(buf));
        mblock_free(&pktbuf_list, buf);
    }
    locker_unlock(&locker);
}


net_err_t pktbuf_add_header(pktbuf_t * buf, int size, int cont)
{
    assert(buf->ref != 0, "buf ref == 0")
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
        block->data = block->payload;
        block->size += resv_size;
        buf->total_size += resv_size;
        size -= resv_size;
        block = pktblk_alloc_list(size, 1);
        if(!block)
        {
            debug_error(DEBUG_PKTBUF, "no buffer %d", size);
            return NET_ERR_NONE;
        }
    }

    pktbuf_insert_blk_list(buf, block, 0);
    display_check_buf(buf);
    return NET_ERR_OK;
}

net_err_t pktbuf_remove_header(pktbuf_t * buf, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    pktblk_t * block = pktbuf_first_blk(buf);

    while (size)
    {
        pktblk_t * next_blk = pktblk_blk_next(block);
        if(size < block->size)
        {
            block->data += size;
            block->size -= size;
            buf->total_size -= size;
            break;
        }
        int curr_size = block->size;
        list_remove_first(&buf->blk_list);
        pktblk_free(block);

        size -= curr_size;
        buf->total_size -= curr_size;

        block = next_blk;
    }

    display_check_buf(buf);
    return NET_ERR_OK;
}

net_err_t pktbuf_resize(pktbuf_t * buf, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    if (size == buf->total_size)
    {
        return NET_ERR_OK;
    }

    if(buf->total_size == 0)
    {
        //alloc size
        pktblk_t * blk = pktblk_alloc_list(size, 0);
        if(!blk)
        {
            debug_error(DEBUG_PKTBUF, "no buffer %d", size);
            return NET_ERR_NONE;
        }
        pktbuf_insert_blk_list(buf, blk, 1);
    }
    else if (size == 0)
    {
        //free all blocks
        pktblk_free_list(pktbuf_first_blk(buf));
        buf->total_size = 0;
        list_init(&buf->blk_list);
    }
    else if (size > buf->total_size)
    {
        pktblk_t * tail_blk = pktbuf_last_blk(buf);
        // The required size of the growth
        int inc_size = size - buf->total_size;
        //tail block's remain size
        int remain_size = curr_blk_tail_free(tail_blk);
        if (remain_size >= inc_size)
        {
            tail_blk->size += inc_size;
            buf->total_size += inc_size;
        }
        else
        {
            //alloc a new block
            pktblk_t * new_blks = pktblk_alloc_list(inc_size - remain_size, 0);
            if(!new_blks)
            {
                debug_error(DEBUG_PKTBUF, "no buffer %d", size);
                return NET_ERR_NONE;
            }
            tail_blk->size += remain_size;
            buf->total_size += remain_size;
            pktbuf_insert_blk_list(buf, new_blks, 1);
        }
    }
    else
    {
        //decrement the total_size
        int total_size = 0;
        pktblk_t * tail_blk;
        for (tail_blk = pktbuf_first_blk(buf); tail_blk; tail_blk = pktblk_blk_next(tail_blk)) {
            total_size += tail_blk->size;
            if (total_size >= size)
            {
                break;
            }
        }
        if (tail_blk == (pktblk_t *)0)
        {
            return NET_ERR_SIZE;
        }

        total_size = 0;
        pktblk_t * curr_blk = pktblk_blk_next(tail_blk);
        while (curr_blk)
        {
            pktblk_t * next = pktblk_blk_next(curr_blk);
            total_size += curr_blk->size;

            list_remove(&buf->blk_list, &curr_blk->node);
            pktblk_free(curr_blk);
            curr_blk = next;
        }
        //set tail block's size
        tail_blk->size -= buf->total_size - total_size - size;
        buf->total_size = size;
    }

    display_check_buf(buf);
    return NET_ERR_OK;
}

net_err_t pktbuf_merge(pktbuf_t * dest, pktbuf_t * src)
{
    assert(dest->ref != 0, "buf ref == 0")
    assert(src->ref != 0, "buf ref == 0")
    pktblk_t * first;

    while ((first = pktbuf_first_blk(src)))
    {
        list_remove_first(&src->blk_list);
        pktbuf_insert_blk_list(dest, first, 1);
    }

    pktbuf_free(src);
    display_check_buf(dest);
    return NET_ERR_OK;
}

net_err_t pktbuf_set_cont(pktbuf_t * buf, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    if(size > buf->total_size)
    {
        debug_error(DEBUG_PKTBUF, "size: %d > total_size: %d", size, buf->total_size);
        return NET_ERR_SIZE;
    }

    if (size > PKTBUF_BLK_SIZE)
    {
        debug_error(DEBUG_PKTBUF, "size: %d > PKTBUF_BLK_SIZE: %d", size, PKTBUF_BLK_SIZE);
        return NET_ERR_SIZE;
    }

    pktblk_t * first_blk = pktbuf_first_blk(buf);
    if (size <= first_blk->size)
    {
        display_check_buf(buf);
        return NET_ERR_OK;
    }

    uint8_t * dest = first_blk->payload;

    for (int i = 0; i < first_blk->size; ++i) {
        *dest++ = first_blk->data[i];
    }
    first_blk->data = first_blk->payload;

    pktblk_t * curr_blk = pktblk_blk_next(first_blk);
    int remain_size = size - first_blk->size;
    while (remain_size && curr_blk)
    {
        int curr_size = (curr_blk->size > remain_size) ? remain_size : curr_blk->size;
        plat_memcpy(dest, curr_blk->data, curr_size);
        dest += curr_size;
        curr_blk->data += curr_size;
        curr_blk->size -= curr_size;
        first_blk->size += curr_size;
        remain_size -= curr_size;
        if (curr_blk->size == 0)
        {
            pktblk_t * next_blk = pktblk_blk_next(curr_blk);
            list_remove(&buf->blk_list, &curr_blk->node);
            pktblk_free(curr_blk);
            curr_blk = next_blk;
        }
    }
    display_check_buf(buf);
    return NET_ERR_OK;
}

void pktbuf_reset_access(pktbuf_t * buf)
{
    if (buf)
    {
        assert(buf->ref != 0, "buf ref == 0")
        buf->pos = 0;
        buf->curr_blk = pktbuf_first_blk(buf);
        if (buf->curr_blk)
            buf->blk_offset = buf->curr_blk->data;
        else
            buf->blk_offset = (uint8_t *)0;
    }
}

/**
 * Move the read-write pointer forward
 */
static void move_forward(pktbuf_t * buf, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    buf->pos += size;
    buf->blk_offset += size;

    pktblk_t * curr = buf->curr_blk;
    if (buf->blk_offset >= curr->data + curr->size)
    {
        // move to next data block
        buf->curr_blk = pktblk_blk_next(curr);
        if (buf->curr_blk)
        {
            buf->blk_offset = buf->curr_blk->data;
        }
        else
        {
            buf->blk_offset = (uint8_t *)0;
        }
    }
}

net_err_t pktbuf_write(pktbuf_t * buf, const uint8_t * src, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    if(!src || !size)
    {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(buf);
    if (remain_size < size)
    {
        debug_error(DEBUG_PKTBUF, "size to big: %d > %d", size, remain_size);
        return NET_ERR_SIZE;
    }

    while (size)
    {
        int blk_size = curr_blk_remain(buf);
        int curr_copy = size > blk_size ? blk_size : size;

        plat_memcpy(buf->blk_offset, src, curr_copy);
        src += curr_copy;
        size -= curr_copy;

        move_forward(buf, curr_copy);
    }

    return NET_ERR_OK;
}

net_err_t pktbuf_read(pktbuf_t * buf, uint8_t * dest, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    if(!dest || !size)
    {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(buf);
    if (remain_size < size)
    {
        debug_error(DEBUG_PKTBUF, "size to big: %d > %d", size, remain_size);
        return NET_ERR_SIZE;
    }

    while (size)
    {
        int blk_size = curr_blk_remain(buf);
        int curr_copy = size > blk_size ? blk_size : size;

        plat_memcpy(dest, buf->blk_offset, curr_copy);
        dest += curr_copy;
        size -= curr_copy;

        move_forward(buf, curr_copy);
    }

    return NET_ERR_OK;
}

net_err_t pktbuf_seek(pktbuf_t * buf, int offset)
{
    assert(buf->ref != 0, "buf ref == 0")
    if (buf->pos == offset)
    {
        return NET_ERR_OK;
    }

    if (offset < 0 || (offset >= buf->total_size))
    {
        return NET_ERR_PARAM;
    }

    int move_bytes;
    if (offset < buf->pos)
    {
        //Move from header block
        buf->curr_blk = pktbuf_first_blk(buf);
        buf->blk_offset = buf->curr_blk->data;
        buf->pos = 0;
        move_bytes = offset;
    }
    else
    {
        //Move from current block
        move_bytes = offset - buf->pos;
    }

    while (move_bytes)
    {
        int remain_size = curr_blk_remain(buf);
        int curr_move = move_bytes > remain_size ? remain_size : move_bytes;

        move_forward(buf, curr_move);
        move_bytes -= curr_move;
    }

    return NET_ERR_OK;
}

net_err_t pktbuf_copy(pktbuf_t * dest, pktbuf_t * src, int size)
{
    assert(dest->ref != 0, "buf ref == 0")
    assert(src->ref != 0, "buf ref == 0")
    if ((size > total_blk_remain(src)) || (size > total_blk_remain(dest)))
    {
        return NET_ERR_SIZE;
    }

    while (size)
    {
        int dest_remain = curr_blk_remain(dest);
        int src_remain = curr_blk_remain(src);
        int copy_size = dest_remain > src_remain ? src_remain : dest_remain;
        copy_size = size > copy_size ? copy_size : size;
        plat_memcpy(dest->blk_offset, src->blk_offset, copy_size);

        move_forward(dest, copy_size);
        move_forward(src, copy_size);
        size -= copy_size;
    }

    return NET_ERR_OK;
}

net_err_t pktbuf_fill(pktbuf_t * buf, uint8_t v, int size)
{
    assert(buf->ref != 0, "buf ref == 0")
    if(!size)
    {
        return NET_ERR_PARAM;
    }

    int remain_size = total_blk_remain(buf);
    if (remain_size < size)
    {
        debug_error(DEBUG_PKTBUF, "size to big: %d > %d", size, remain_size);
        return NET_ERR_SIZE;
    }

    while (size)
    {
        int blk_size = curr_blk_remain(buf);
        int curr_fill = size > blk_size ? blk_size : size;

        plat_memset(buf->blk_offset, v, curr_fill);
        size -= curr_fill;

        move_forward(buf, curr_fill);
    }

    return NET_ERR_OK;
}

void pktbuf_inc_ref(pktbuf_t * buf)
{
    locker_lock(&locker);
    buf->ref++;
    locker_unlock(&locker);
}

uint16_t pktbuf_checksum16(pktbuf_t * buf, int len, int pre_sum, int complement)
{
    assert(buf->ref != 0, "buf ref == 0")

    int remain_size = total_blk_remain(buf);
    if (remain_size < len)
    {
        debug_warn(DEBUG_PKTBUF, "len too big");
        return 0;
    }

    uint16_t sum = pre_sum;
    while (len > 0)
    {
        int blk_size = curr_blk_remain(buf);
        int curr_size = (blk_size > len) ? len : blk_size;

        sum = checksum16(buf->blk_offset, curr_size, sum, 0);
        move_forward(buf, curr_size);
        len -= curr_size;
    }
    return complement ? (uint16_t)~sum : sum;
}