#include "net.h"
#include <sys_plat.h>
#include "netif_pcap.h"
#include "debug.h"
#include "list.h"
#include "mblock.h"
#include "pktbuf.h"

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


void mblock_test() {
    mblock_t block;
    static uint8_t buffer[100][10];

    //10 memory blocks, every block 100 size
    mblock_init(&block, buffer, 100, 10, LOCKER_THREAD);
    void * temp[10];
    for (int i = 0; i < 10; ++i) {
        temp[i] = mblock_alloc(&block, 10);
        plat_printf("blcok: %p, free count: %d\n", temp[i], mblock_free_cnt(&block));
    }

    for (int i = 0; i < 10; ++i) {
        mblock_free(&block, temp[i]);
        plat_printf("free count: %d\n", mblock_free_cnt(&block));
    }

    mblock_destroy(&block);
}

void pktbuf_test()
{
    pktbuf_t * buf = pktbuf_alloc(2000);
    pktbuf_free(buf);

    buf = pktbuf_alloc(2000);
    for (int i = 0; i < 10; ++i) {
        pktbuf_add_header(buf, 33, 1);
    }

    for (int i = 0; i < 10; ++i) {
        pktbuf_remove_header(buf, 33);
    }
}

void test()
{
    debug_info(DEBUG_TEST, "hello");
    debug_warn(DEBUG_TEST, "hello");
    debug_error(DEBUG_TEST, "hello");

    assert(3==3, "test assert")
    list_test();
    mblock_test();
    pktbuf_test();
}

int main()
{
    net_init();
    net_start();
    net_dev_init();

    test();

    for(;;)
    {
        sys_sleep(10);
    }
}