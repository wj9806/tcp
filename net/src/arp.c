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

#if DEBUG_DISP_ENABLED(DEBUG_ARP)
static void arp_pkt_display(arp_pkt_t * packet)
{
    uint16_t opcode = x_ntohs(packet->opcode);
    plat_printf("-----------arp packet--------------\n");
    plat_printf("   htype:%d\n", x_ntohs(packet->htype));
    plat_printf("   ptype:%04x\n", x_ntohs(packet->ptype));
    plat_printf("   hwlen:%d\n", packet->hwlen);
    plat_printf("   plen:%d\n", packet->plen);
    plat_printf("   type:%d\n", opcode);

    switch (opcode) {
        case ARP_REQUEST:
            plat_printf("request\n");
            break;
        case ARP_REPLY:
            plat_printf("reply\n");
            break;
        default:
            plat_printf("unknown\n");
    }

    debug_dump_ip_buf("     sender:", packet->sender_paddr);
    debug_dump_hwaddr("     mac:", packet->sender_hwaddr, ETHER_HWA_SIZE);
    debug_dump_ip_buf("\n     target:", packet->target_paddr);
    debug_dump_hwaddr("     mac:", packet->target_hwaddr, ETHER_HWA_SIZE);
    plat_printf("\n");
}

static void display_arp_entry(arp_entry_t * entry)
{
    plat_printf("%d: ", (int)(entry - cache_tbl));
    debug_dump_ip_buf(" ip: ", entry->paddr);
    debug_dump_hwaddr(" mac: ", entry->hwaddr, ETHER_HWA_SIZE);

    plat_printf("tmo: %d, retry: %d, %s, buf: %d\n",
                entry->tmo, entry->retry, entry->state == NET_ARP_RESOLVED ? "stable" : "pending", list_count(&entry->buf_list));
}

static void display_arp_tbl(void)
{
    plat_printf("-----------arp table--------------\n");
    arp_entry_t * entry = cache_tbl;

    for (int i = 0; i < ARP_TABLE_SIZE; ++i) {
        if (entry->state != NET_ARP_FREE)
        {
            continue;
        }

        display_arp_entry(entry);
    }
}

#else
#define arp_pkt_display(packet)
#define display_arp_tbl()
#endif

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

static net_err_t is_pkt_ok(arp_pkt_t * arp_packet, uint16_t size, netif_t * netif)
{
    if (size < sizeof(arp_pkt_t))
    {
        debug_warn(DEBUG_ARP, "packet size error");
        return NET_ERR_SIZE;
    }

    if ((x_ntohs(arp_packet->htype) != ARP_HW_ETHER)
        || (arp_packet->hwlen != ETHER_HWA_SIZE)
        || (x_htons(arp_packet->ptype) != NET_PROTOCOL_IPV4)
        || (arp_packet->plen != IPV4_ADDR_SIZE))
    {
        debug_warn(DEBUG_ARP, "packet error");
        return NET_ERR_NOT_SUPPORT;
    }

    uint16_t opcode = x_ntohs(arp_packet->opcode);
    if (opcode != ARP_REPLY && opcode != ARP_REQUEST)
    {
        debug_warn(DEBUG_ARP, "unknown opcode");
        return NET_ERR_NOT_SUPPORT;
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

    arp_pkt_display(arp_pkt);
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

net_err_t arp_make_reply(netif_t * netif, pktbuf_t * buf)
{
    arp_pkt_t * arp_packet = (arp_pkt_t *) pktbuf_data(buf);
    arp_packet->opcode = x_htons(ARP_REPLY);

    plat_memcpy(arp_packet->target_hwaddr, arp_packet->sender_hwaddr, ETHER_HWA_SIZE);
    plat_memcpy(arp_packet->target_paddr, arp_packet->sender_paddr, IPV4_ADDR_SIZE);

    plat_memcpy(arp_packet->sender_hwaddr, netif->hwaddr.addr, ETHER_HWA_SIZE);
    ipaddr_to_buf(&netif->ipaddr, arp_packet->sender_paddr);
    arp_pkt_display(arp_packet);
    return ether_raw_out(netif, NET_PROTOCOL_ARP, arp_packet->target_hwaddr, buf);
}


net_err_t arp_in(netif_t * netif, pktbuf_t * buf)
{
    debug_info(DEBUG_ARP, "arp in");
    net_err_t err = pktbuf_set_cont(buf, sizeof(arp_pkt_t));
    if (err < 0)
    {
        return err;
    }
    arp_pkt_t * arp_packet = (arp_pkt_t *) pktbuf_data(buf);
    err = is_pkt_ok(arp_packet, buf->total_size, netif);
    if (err != NET_ERR_OK)
    {
        return err;
    }
    if (x_ntohs(arp_packet->opcode) == ARP_REQUEST)
    {
        debug_info(DEBUG_ARP, "arp request, send reply");
        return arp_make_reply(netif, buf);
    }
    pktbuf_free(buf);
    return NET_ERR_OK;
}