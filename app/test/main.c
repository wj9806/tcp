#include "net.h"
#include <sys_plat.h>
#include "netif_pcap.h"
#include "debug.h"
#include "list.h"

#define DEBUG_TEST    DEBUG_LEVEL_INFO

typedef struct _tnode_t
{
    int id;
    node_t node;
} tnode_t;

void list_test()
{
    tnode_t tnode[4];
    list_t list;
    list_init(&list);
    for (int i = 0; i < 4; ++i) {
        tnode[i].id = i;
        list_insert_first(&list, &tnode[i].node);

    }

    node_t * node;
    for ((node) = (&list)->first; (node); (node) = (node)->next)
    {
        tnode_t * tnode = list_node_parent(node, tnode_t, node);
        plat_printf("%d\n", tnode->id);
    }
}

net_err_t net_dev_init()
{
    netif_pcap_open();

    return NET_ERR_OK;
}


int main()
{
    debug_info(DEBUG_TEST, "hello");
    debug_warn(DEBUG_TEST, "hello");
    debug_error(DEBUG_TEST, "hello");

    assert(3==3, "test assert");
    list_test();

    net_init();
    net_start();

    net_dev_init();

    for(;;)
    {
        sys_sleep(10);
    }
}