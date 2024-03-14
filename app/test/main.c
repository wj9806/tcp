#include "net.h"
#include <sys_plat.h>
#include "netif_pcap.h"
#include "debug.h"

net_err_t net_dev_init()
{
    netif_pcap_open();

    return NET_ERR_OK;
}

#define DEBUG_TEST    DEBUG_LEVEL_INFO


int main()
{
    debug_info(DEBUG_TEST, "hello");
    debug_warn(DEBUG_TEST, "hello");
    debug_error(DEBUG_TEST, "hello");

    assert(3==3, "test assert");
    assert(3==0, "test assert");

    net_init();
    net_start();

    net_dev_init();

    for(;;)
    {
        sys_sleep(10);
    }
}