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
