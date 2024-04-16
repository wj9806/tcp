//
// Created by wj on 2024/4/12.
//

#include "arp.h"
#include "mblock.h"
#include "tools.h"
#include "protocol.h"

static arp_entry_t cache_tbl[ARP_TABLE_SIZE];
static mblock_t cache_block;
static list_t cache_list;

static net_err_t arp_cache_init(void)
{
    list_init(&cache_list);
    net_err_t err = mblock_init(&cache_block, cache_tbl, sizeof(arp_entry_t), ARP_TABLE_SIZE, LOCKER_NONE);
    if (err < 0)
    {
        return err;
    }
    return NET_ERR_OK;
}

net_err_t arp_init()
{
    debug_info(DEBUG_ARP, "init arp");
    net_err_t err = arp_cache_init();
    if (err < 0)
    {
        debug_error(DEBUG_ARP, "init arp cache failed");
        return err;
    }

    return NET_ERR_OK;
}

net_err_t arp_make_request(netif_t * netif, const ipaddr_t * dest)
{
    pktbuf_t * buf = pktbuf_alloc(sizeof(arp_pkt_t));
    if (buf == (pktbuf_t *) 0)
    {
        debug_error(DEBUG_ARP, "alloc pktbuf failed");
        return NET_ERR_NONE;
    }

    pktbuf_set_cont(buf, sizeof(arp_pkt_t));

    arp_pkt_t * arp_pkt = (arp_pkt_t *) pktbuf_data(buf);
    arp_pkt->htype = x_htons(ARP_HW_ETHER);
    arp_pkt->ptype = x_htons(NET_PROTOCOL_IPV4);
    arp_pkt->hwlen = ETHER_HWA_SIZE;
    arp_pkt->plen = IPV4_ADDR_SIZE;
    arp_pkt->opcode = x_htons(ARP_REQUEST);
    plat_memcpy(arp_pkt->sender_hwaddr, netif->hwaddr.addr, ETHER_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_pkt->sender_paddr);
    plat_memset(arp_pkt->target_hwaddr, 0, ETHER_HWA_SIZE);
    ipaddr_to_buf(dest, arp_pkt->target_paddr);

    net_err_t err = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
    if (err < 0)
    {
        pktbuf_free(buf);
    }
    return err;
}

net_err_t arp_make_gratuitous(netif_t * netif)
{
    return arp_make_request(netif, &netif->ipaddr);
}