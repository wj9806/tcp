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

static void insert_timer(net_timer_t * insert)
{
    node_t * node;
    list_for_each(node, &timer_list)
    {
        net_timer_t * curr = list_node_parent(node, net_timer_t, node);
        if (insert->curr > curr->curr)
        {
            insert->curr -= curr->curr;
        }
        else if(insert->curr == curr->curr)
        {
            insert->curr = 0;
            list_insert_after(&timer_list, node, &insert->node);
            return;
        }
        else
        {
            curr->curr -= insert->curr;
            node_t * pre = list_node_pre(&curr->node);
            if (pre)
            {
                list_insert_after(&timer_list, pre, &insert->node);
            }
            else
            {
                list_insert_first(&timer_list, &insert->node);
            }
            return;
        }
    }

    list_insert_last(&timer_list, &insert->node);
}

net_err_t net_timer_add(net_timer_t * timer, const char * name, timer_proc_t proc, void * arg, int ms, int flags)
{
    plat_strncpy(timer->name, name, TIMER_NAME_SIZE);
    timer->reload = timer->curr = ms;
    timer->proc = proc;
    timer->arg = arg;
    timer->flags = flags;
    insert_timer(timer);
    display_timer_list();
    return NET_ERR_OK;
}

void net_timer_remove(net_timer_t * timer)
{
    node_t * node;
    list_for_each(node, &timer_list)
    {
        net_timer_t * curr = list_node_parent(node, net_timer_t, node);
        if (curr != timer)
        {
            continue;
        }
        node_t * next = list_node_next(&timer->node);
        if (next)
        {
            net_timer_t * next_timer = list_node_parent(next, net_timer_t, node);
            next_timer->curr += timer->curr;
        }
        list_remove(&timer_list, &timer->node);
        break;
    }

    display_timer_list();
}

net_err_t net_timer_check_tmo(int diff_ms)
{
    list_t wait_list;
    list_init(&wait_list);

    node_t * node = list_first(&timer_list);
    while (node)
    {
        node_t * next = list_node_next(node);
        net_timer_t * timer = list_node_parent(node, net_timer_t , node);
        if (timer->curr > diff_ms)
        {
            timer->curr -= diff_ms;
            break;
        }
        diff_ms -= timer->curr;
        timer->curr = 0;
        list_remove(&timer_list, &timer->node);
        list_insert_last(&wait_list, &timer->node);

        node = next;
    }

    while ((node = list_remove_first(&wait_list)) != (node_t *) 0)
    {
        net_timer_t * timer = list_node_parent(node, net_timer_t , node);
        if (timer)
        {
            timer->proc(timer, timer->arg);
            if (timer->flags & NET_TIMER_RELOAD)
            {
                timer->curr = timer->reload;
                insert_timer(timer);
            }
        }
    }
    return NET_ERR_OK;
}

int net_timer_first_tmo(void)
{
    node_t * node = list_first(&timer_list);
    if (node)
    {
        net_timer_t * timer = list_node_parent(node, net_timer_t , node);
        return timer->curr;
    }
    return 0;
}