#include "net.h"
#include "exmsg.h"
#include "net_plat.h"
#include "pktbuf.h"
#include "netif.h"

net_err_t net_init(void)
{
    debug_info(DEBUG_INIT, "init net");
    net_plat_init();
    exmsg_init();
    pktbuf_init();
    netif_init();
    return NET_ERR_OK;
}

net_err_t net_start(void)
{
    exmsg_start();
    debug_info(DEBUG_INIT, "net is running.");
    return NET_ERR_OK;
}