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

    plat_printf("----------------------header add.\n");
    for (int i = 0; i < 10; ++i) {
        pktbuf_add_header(buf, 33, 0);
    }

    for (int i = 0; i < 10; ++i) {
        pktbuf_remove_header(buf, 33);
    }

    pktbuf_free(buf);

    buf = pktbuf_alloc(8);
    pktbuf_resize(buf, 32);
    pktbuf_resize(buf, 288);
    pktbuf_resize(buf, 4196);
    pktbuf_resize(buf, 1920);
    pktbuf_resize(buf, 288);
    pktbuf_resize(buf, 0);
    pktbuf_free(buf);

    buf = pktbuf_alloc(689);
    pktbuf_t * sbuf = pktbuf_alloc(892);
    pktbuf_merge(buf, sbuf);
    pktbuf_free(buf);

    buf = pktbuf_alloc(32);
    pktbuf_merge(buf, pktbuf_alloc(4));
    pktbuf_merge(buf, pktbuf_alloc(16));
    pktbuf_merge(buf, pktbuf_alloc(32));
    pktbuf_merge(buf, pktbuf_alloc(64));

    pktbuf_set_cont(buf, 44);
    pktbuf_set_cont(buf, 60);
    pktbuf_set_cont(buf, 200);
    pktbuf_free(buf);

    buf = pktbuf_alloc(32);
    pktbuf_merge(buf, pktbuf_alloc(4));
    pktbuf_merge(buf, pktbuf_alloc(16));
    pktbuf_merge(buf, pktbuf_alloc(54));
    pktbuf_merge(buf, pktbuf_alloc(32));
    pktbuf_merge(buf, pktbuf_alloc(38));
    pktbuf_merge(buf, pktbuf_alloc(512));

    pktbuf_reset_access(buf);

    static uint16_t temp[1000];
    for (int i = 0; i < 1000; ++i) {
        temp[i] = i;
    }
    pktbuf_write(buf, (uint8_t *) temp, pktbuf_total(buf));

    //reset read-write pointer
    pktbuf_reset_access(buf);
    static uint16_t read_temp[1000];
    plat_memset(read_temp, 0, sizeof(read_temp));
    pktbuf_read(buf, (uint8_t *) read_temp, pktbuf_total(buf));
    if (plat_memcmp(temp, read_temp, pktbuf_total(buf)) != 0)
    {
        plat_printf("read failed, not eq");
        return;
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