#include "net.h"
#include <sys_plat.h>
#include "netif_pcap.h"
#include "debug.h"
#include "list.h"
#include "mblock.h"
#include "pktbuf.h"
#include "netif.h"
#include "timer.h"
#include "ping/ping.h"
#include "exmsg.h"

#define DEBUG_TEST    DEBUG_LEVEL_INFO

typedef struct
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

pcap_data_t netdev0_data = {.ip = netdev0_phy_ip, .hwaddr = netdev0_hwaddr};

net_err_t netdev_init()
{
    netif_t * netif = netif_open("netif 0", &netdev_ops, &netdev0_data);
    if (!netif)
    {
        debug_error(DEBUG_NETIF, "open netif error");
        return NET_ERR_NONE;
    }

    ipaddr_t ip, mask, gw;
    ipaddr_from_str(&ip, netdev0_ip);
    ipaddr_from_str(&mask, netdev0_mask);
    ipaddr_from_str(&gw, netdev0_gw);

    netif_set_addr(netif, &ip, &mask, &gw);
    netif_set_active(netif);

    pktbuf_t * buf = pktbuf_alloc(32);
    pktbuf_fill(buf, 0x53, 32);
    ipaddr_t dest, src;
    ipaddr_from_str(&dest, friend0_ip);
    ipaddr_from_str(&src, netdev0_ip);
    //netif_out(netif, &addr, buf);
    //ipv4_out(0, &dest, & src, buf);

    //ipaddr_from_str(&addr, "192.168.74.255");
    //buf = pktbuf_alloc(32);
    //pktbuf_fill(buf, 0xA5, 32);
    //netif_out(netif, &addr, buf);
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
        plat_printf("read failed, not eq\n");
        return;
    }

    plat_memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(buf, 85 * 2);
    pktbuf_read(buf, (uint8_t *) read_temp, 256);
    if (plat_memcmp(temp + 85, read_temp, 256) != 0)
    {
        plat_printf("read failed, not eq\n");
        return;
    }

    pktbuf_t * dest = pktbuf_alloc(1024);
    pktbuf_seek(dest, 600);
    pktbuf_seek(buf, 200);

    pktbuf_copy(dest, buf, 120);
    plat_memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(dest, 600);
    pktbuf_read(dest, (uint8_t *) read_temp, 120);
    if (plat_memcmp(temp + 100, read_temp, 120) != 0)
    {
        plat_printf("copy failed, not eq\n");
        return;
    }

    pktbuf_seek(dest, 0);
    pktbuf_fill(dest, 53, pktbuf_total(dest));
    plat_memset(read_temp, 0, sizeof(read_temp));
    pktbuf_seek(dest, 0);
    pktbuf_read(dest, (uint8_t *)read_temp, pktbuf_total(dest));
    char * c = (char *) read_temp;
    for (int i = 0; i < pktbuf_total(dest); ++i) {
        if (*c++ != 53)
        {
            plat_printf("fill failed, not eq\n");
            return;
        }
    }

    pktbuf_free(dest);
    pktbuf_free(buf);
}

void timer0_proc(struct net_timer_t * timer, void * arg)
{
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count);
}

void timer1_proc(struct net_timer_t * timer, void * arg)
{
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

void timer2_proc(struct net_timer_t * timer, void * arg)
{
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

void timer3_proc(struct net_timer_t * timer, void * arg)
{
    static int count = 1;
    printf("this is %s: %d\n", timer->name, count++);
}

void timer_test()
{
    static net_timer_t t0, t1, t2, t3;
    net_timer_add(&t0, "t0", timer0_proc, (void *)0, 200, 0);
    net_timer_add(&t3, "t3", timer3_proc, (void *)0, 4000, NET_TIMER_RELOAD);
    net_timer_add(&t1, "t1", timer1_proc, (void *)0, 1000, NET_TIMER_RELOAD);
    net_timer_add(&t2, "t2", timer2_proc, (void *)0, 1000, NET_TIMER_RELOAD);

    net_timer_remove(&t0);
}

void test()
{
#ifdef TEST
    debug_info(DEBUG_TEST, "hello");
    debug_warn(DEBUG_TEST, "hello");
    debug_error(DEBUG_TEST, "hello");

    assert(3==3, "test assert")
    list_test();
    mblock_test();
    pktbuf_test();
    //netif_t * netif = netif_open("pcap");
    timer_test();
#endif

}

int main()
{
    net_init();
    netdev_init();
    net_start();

    int arg = 0x12345;
    net_err_t err = exmsg_func_exec(test_func, &arg);

    ping_t ping;
    ping_run(&ping, friend0_ip, 4, 64, 1000);
    char cmd[32], param[32];
    for(;;)
    {
        printf(">>");
        scanf("%s%s", cmd, param);
        if (strcmp(cmd, "ping") == 0)
        {
            ping_run(&ping, param, 4, 64, 1000);
        }
        sys_sleep(10);
    }
}