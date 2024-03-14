#include <stdio.h>
#include "net.h"
#include <sys_plat.h>
#include "netif_pcap.h"

net_err_t net_dev_init()
{
    netif_pcap_open();

    return NET_ERR_OK;
}

int main()
{
    net_init();
    net_start();

    net_dev_init();

    for(;;)
    {
        sys_sleep(10);
    }
}