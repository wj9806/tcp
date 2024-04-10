//
// Created by wj on 2024/4/10.
//

#include "timer.h"
#include "debug.h"
#include "sys_plat.h"

static list_t timer_list;

#if DEBUG_DISP_ENABLED(DEBUG_TIMER)
static void display_timer_list()
{
    plat_printf("--------------- timer list ---------------\n");
    node_t * node;
    int index = 0;
    list_for_each(node, &timer_list)
    {
        net_timer_t * timer = list_node_parent(node, net_timer_t, node);
        plat_printf("%d: %s, period: %d, curr: %dms, reload: %d ms \n", index++, timer->name,
                    timer->flags & NET_TIMER_RELOAD ? 1 : 0, timer->curr, timer->reload);
    }
}
#else
#define display_timer_list()
#endif

net_err_t net_timer_init(void)
{
    debug_info(DEBUG_TIMER, "timer init");
    list_init(&timer_list);
    return NET_ERR_OK;
}

net_err_t net_timer_add(net_timer_t * timer, const char * name, timer_proc_t proc, void * arg, int ms, int flags)
{
    plat_strncpy(timer->name, name, TIMER_NAME_SIZE);
    timer->reload = timer->curr = ms;
    timer->proc = proc;
    timer->arg = arg;
    timer->flags = flags;
    node_init(&timer->node);
    list_insert_last(&timer_list, &timer->node);
    display_timer_list();
    return NET_ERR_OK;
}
