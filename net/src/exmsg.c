//
// Created by xsy on 2024/3/14.
//
#include "exmsg.h"
#include "sys_plat.h"
#include "fixq.h"
#include "debug.h"
#include "mblock.h"

static void * msg_tbl[EXMSG_MSG_CNT];
static fixq_t msg_queue;

static exmsg_t msg_buffer[EXMSG_MSG_CNT];
static mblock_t msg_block;

net_err_t exmsg_init(void)
{
    net_err_t err = fixq_init(&msg_queue, msg_tbl, EXMSG_MSG_CNT, EXMSG_LOCKER);
    if (err < 0)
    {
        debug_error(DEBUG_MSG, "fixq init failed");
        return err;
    }

    err = mblock_init(&msg_block,msg_buffer, sizeof(exmsg_t), EXMSG_MSG_CNT, EXMSG_LOCKER);
    if (err < 0)
    {
        debug_error(DEBUG_MSG, "mblock init failed");
        return err;
    }
    debug_info(DEBUG_MSG, "exmsg init done");
    return NET_ERR_OK;
}

static void work_thread(void * arg)
{
    debug_info(DEBUG_MSG, "exmsg is running\n");

    while (1)
    {
        exmsg_t * msg = (exmsg_t *) fixq_recv(&msg_queue, 0);
        if (msg != (exmsg_t *)0) {
            plat_printf("recv a msg: %d\n", msg->id);

            mblock_free(&msg_block, msg);
        }
    }
}

net_err_t exmsg_netif_in(netif_t * netif)
{
    exmsg_t * msg = mblock_alloc(&msg_block, -1);
    if (!msg)
    {
        debug_warn(DEBUG_MSG, "no free msg");
        return NET_ERR_MEM;
    }

    static int id = 0;
    msg->type = NET_EXMSG_NETIF_IN;
    msg->id = id++;
    net_err_t err = fixq_send(&msg_queue, msg, -1);
    if (err < 0)
    {
        debug_warn(DEBUG_MSG, "fixq full");
        mblock_free(&msg_block, msg);
        return err;
    }

    return err;
}

net_err_t exmsg_start(void)
{
    sys_thread_t thread = sys_thread_create(work_thread, (void *)0);
    if (thread == SYS_THREAD_INVALID)
    {
        return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}