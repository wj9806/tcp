//
// Created by wj on 2024/3/20.
//

#ifndef NET_PKTBUF_H
#define NET_PKTBUF_H

#include "list.h"
#include <stdint.h>
#include "net_cfg.h"
#include "net_err.h"
#include "sys_plat.h"

//data block struct
typedef struct pktblk_t
{
    node_t node;
    //data used size
    int size;
    //data addr
    uint8_t * data;
    uint8_t payload[PKTBUF_BLK_SIZE];
} pktblk_t;


typedef struct pktbuf_t
{
    int total_size;
    //data block list, which save pktblk_t's node
    list_t blk_list;
    //data packet node
    node_t node;
} pktbuf_t;

net_err_t pktbuf_init(void);

//alloc given size pktbuf_t
pktbuf_t * pktbuf_alloc(int size);

//free pktbuf_t
void pktbuf_free(pktbuf_t * buf);

/**
 * @param block given pktblk_t
 * @return given block's next pktblk_t
 */
static inline pktblk_t * pktblk_blk_next(pktblk_t * block)
{
    node_t * next = list_node_next(&block->node);
    return list_node_parent(next, pktblk_t, node);
}

static inline int curr_blk_tail_free(pktblk_t * blk)
{
    return (int) ((blk->payload + PKTBUF_BLK_SIZE) - (blk->data + blk->size));
}

/**
 * @param buf given pktbuf_t
 * @return given pktbuf_t's first pktblk_t
 */
static inline pktblk_t * pktbuf_first_blk(pktbuf_t * buf)
{
    node_t * first = list_first(&buf->blk_list);
    return list_node_parent(first, pktblk_t, node);
}

/**
 * @param buf given pktbuf_t
 * @return given pktbuf_t's last pktblk_t
 */
static inline pktblk_t * pktbuf_last_blk(pktbuf_t * buf)
{
    node_t * last = list_last(&buf->blk_list);
    return list_node_parent(last, pktblk_t, node);
}

/**
 * debug structure of given pktbuf_t
 */
#if DEBUG_DISP_ENABLED(DEBUG_PKTBUF)
static void display_check_buf(pktbuf_t * buf)
{
    if (!buf)
    {
        debug_error(DEBUG_PKTBUF, "invalid buf");
    }

    plat_printf("buf: %p: size %d\n", buf, buf->total_size);
    pktblk_t * curr;
    int index = 0, total_size = 0;
    for (curr = pktbuf_first_blk(buf); curr; curr = pktblk_blk_next(curr))
    {
        plat_printf("%-*d: ",3, index++);
        int pre_size = (int)(curr->data - curr->payload);
        plat_printf("pre: %-*d b, ", 3, pre_size);

        if ((curr->data < curr->payload) || (curr->data > curr->payload + PKTBUF_BLK_SIZE))
        {
            debug_error(DEBUG_PKTBUF, "bad block data\n");
        }

        int used_size = curr->size;
        plat_printf("used: %-*d b, ", 3, used_size);

        int free_size = curr_blk_tail_free(curr);
        plat_printf("free: %-*d b\n", 3, free_size);

        int blk_total = pre_size + used_size + free_size;
        if (blk_total != PKTBUF_BLK_SIZE)
        {
            debug_error(DEBUG_PKTBUF, "bad block size : %d != %d\n", blk_total, PKTBUF_BLK_SIZE);
        }
        total_size += used_size;
    }

    if (total_size != buf->total_size)
    {
        debug_error(DEBUG_PKTBUF, "bad buf size: %d != %d\n", total_size, buf->total_size);
    }
}
#else
#define display_check_buf(buf)
#endif

/**
 * @param buf buffer
 * @param size header size
 * @param cout Whether it is continuous
 */
net_err_t pktbuf_add_header(pktbuf_t * buf, int size, int cont);

/**
 * remove the pktbuf's header
 */
net_err_t pktbuf_remove_header(pktbuf_t * buf, int size);

/**
 * resize the pktbuf's size
 * @param size desired size
 */
net_err_t pktbuf_resize(pktbuf_t * buf, int size);

/**
 * merge src pktbuf_t into dest pktbuf_t
 * @param dest dest pktbuf_t
 * @param src src pktbuf_t
 */
net_err_t pktbuf_merge(pktbuf_t * dest, pktbuf_t * src);

/**
 * set pktbuf's header continuous
 * @param size Byte size to be merged
 */
net_err_t pktbuf_set_cont(pktbuf_t * buf, int size);

#endif //NET_PKTBUF_H