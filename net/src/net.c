#include "net.h"
#include "exmsg.h"
#include "net_plat.h"
#include "pktbuf.h"
#include "netif.h"
#include "loop.h"
#include "ether.h"
#include "tools.h"
#include "timer.h"
#include "arp.h"
#include "ipv4.h"
#include "icmpv4.h"
#include "sock.h"
#include "raw.h"
#include "udp.h"
#include "tcp.h"
#include "dns.h"

net_err_t net_init(void)
{
    debug_info(DEBUG_INIT, "init net");
    net_plat_init();
    tools_init();
    exmsg_init();
    pktbuf_init();
    netif_init();
    net_timer_init();
    ether_init();
    arp_init();
    ipv4_init();
    icmpv4_init();
    socket_init();
    raw_init();
    udp_init();
    tcp_init();
    dns_init();
    loop_init();
    return NET_ERR_OK;
}

net_err_t net_start(void)
{
    exmsg_start();
    debug_info(DEBUG_INIT, "net is running.");
    return NET_ERR_OK;
}