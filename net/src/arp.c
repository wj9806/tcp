//
// Created by wj on 2024/4/12.
//

#include "arp.h"
#include "mblock.h"
#include "tools.h"
#include "protocol.h"
#include "timer.h"

#define to_scan_cnt(tmo)    (tmo/ARP_TIMER_TMO)

static net_timer_t cache_timer;
static arp_entry_t cache_tbl[ARP_TABLE_SIZE];
static mblock_t cache_block;
static list_t cache_list;
static uint8_t empty_hwaddr[] = {0, 0, 0, 0, 0, 0};

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

    plat_printf("  tmo: %d, retry: %d, %s, buf: %d\n",
                entry->tmo, entry->retry, entry->state == NET_ARP_RESOLVED ? "stable" : "pending", list_count(&entry->buf_list));
}

static void display_arp_tbl(void)
{
    plat_printf("-----------arp table--------------\n");
    arp_entry_t * entry = cache_tbl;

    for (int i = 0; i < ARP_TABLE_SIZE; ++i, entry++) {
        if ((entry->state != NET_ARP_WAITING) && (entry->state != NET_ARP_RESOLVED))
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

/**
 * clear entry's buffer
 */
static void cache_clear_all(arp_entry_t * entry)
{
    node_t * first;
    while ((first = list_remove_first(&entry->buf_list)))
    {
        pktbuf_t * buf = list_node_parent(first, pktbuf_t, node);
        pktbuf_free(buf);
    }
}

static net_err_t cache_send_all(arp_entry_t * entry)
{
    node_t * first;
    while ((first = list_remove_first(&entry->buf_list)))
    {
        pktbuf_t * buf = list_node_parent(first, pktbuf_t, node);

        net_err_t err = ether_raw_out(entry->netif, NET_PROTOCOL_IPV4, entry->hwaddr, buf);
        if (err < 0)
        {
            pktbuf_free(buf);
        }
    }
    return NET_ERR_OK;
}

static arp_entry_t * cache_alloc(int force)
{
    arp_entry_t * entry = mblock_alloc(&cache_block, -1);
    if (!entry && force)
    {
        node_t * node = list_remove_first(&cache_list);
        if (!node)
        {
            debug_warn(DEBUG_ARP, "alloc arp entry failed");
            return (arp_entry_t *)0;
        }
        entry = list_node_parent(node, arp_entry_t, node);
        cache_clear_all(entry);
    }

    if (entry)
    {
        plat_memset(entry, 0, sizeof(arp_entry_t));
        entry->state = NET_ARP_FREE;
        node_init(&entry->node);
        list_init(&entry->buf_list);
    }

    return entry;
}

/**
 * free the given arp_entry_t
 */
static void cache_free(arp_entry_t * entry)
{
    cache_clear_all(entry);
    list_remove(&cache_list, &entry->node);
    mblock_free(&cache_block, entry);
}

static arp_entry_t * cache_find(uint8_t * ip)
{
    node_t * node;
    list_for_each(node, &cache_list)
    {
        arp_entry_t * entry = list_node_parent(node, arp_entry_t, node);
        if (plat_memcmp(ip, entry->paddr, IPV4_ADDR_SIZE) == 0)
        {
            list_remove(&cache_list, &entry->node);
            list_insert_first(&cache_list, &entry->node);
            return entry;
        }
    }
    return (arp_entry_t *)0;
}

static void cache_entry_set(arp_entry_t * entry, uint8_t * hwaddr, uint8_t * ip, netif_t * netif, int state)
{
    plat_memcpy(entry->hwaddr, hwaddr, ETHER_HWA_SIZE);
    plat_memcpy(entry->paddr, ip, IPV4_ADDR_SIZE);
    entry->state = state;
    entry->netif = netif;
    if (state == NET_ARP_RESOLVED)
    {
        entry->tmo = to_scan_cnt(ARP_ENTRY_STABLE_TMO);
    }
    else
    {
        entry->retry = to_scan_cnt(ARP_ENTRY_PENDING_TMO);
    }
}

static net_err_t cache_insert(netif_t * netif, uint8_t * ip, uint8_t * hwaddr, int force)
{
    if (*(uint32_t*)ip == 0)
    {
        return NET_ERR_NOT_SUPPORT;
    }
    arp_entry_t * entry = cache_find(ip);
    if (!entry)
    {
        entry = cache_alloc(force);
        if (!entry)
        {
            debug_dump_ip_buf("alloc failed. ip:", ip);
            return NET_ERR_NONE;
        }
        cache_entry_set(entry, hwaddr, ip, netif, NET_ARP_RESOLVED);
        list_insert_first(&cache_list, &entry->node);
    }
    else
    {
        cache_entry_set(entry, hwaddr, ip, netif, NET_ARP_RESOLVED);
        if (list_first(&cache_list) != &entry->node)
        {
            list_remove(&cache_list, &entry->node);
            list_insert_first(&cache_list, &entry->node);
        }

        net_err_t err = cache_send_all(entry);
        if (err < 0)
        {
            debug_error(DEBUG_ARP, "send arp packet failed.");
            return err;
        }
    }
    display_arp_tbl();
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

static void arp_cache_tmo(net_timer_t * timer, void * arg)
{
    node_t * curr, * next;

    for (curr = cache_list.first; curr; curr = next)
    {
        next = list_node_next(curr);
        arp_entry_t * entry = list_node_parent(curr, arp_entry_t, node);
        if (--entry->tmo > 0)
        {
            continue;
        }

        switch (entry->state) {
            case NET_ARP_RESOLVED:
                display_arp_entry(entry);
                ipaddr_t ipaddr;
                ipaddr_from_buf(&ipaddr, entry->paddr);
                entry->state = NET_ARP_WAITING;
                entry->tmo = to_scan_cnt(ARP_ENTRY_PENDING_TMO);
                entry->retry = ARP_ENTRY_RETRY_CNT;
                arp_make_request(entry->netif, &ipaddr);
                break;
            case NET_ARP_WAITING:
                if (--entry->retry == 0)
                {
                    debug_info(DEBUG_ARP, "retry pending >= %d", ARP_ENTRY_PENDING_TMO);
                    display_arp_entry(entry);
                    cache_free(entry);
                }
                else
                {
                    debug_info(DEBUG_ARP, "arp retry pending");
                    ipaddr_t ipaddr;
                    ipaddr_from_buf(&ipaddr, entry->paddr);
                    entry->tmo = to_scan_cnt(ARP_ENTRY_PENDING_TMO);
                    arp_make_request(entry->netif, &ipaddr);
                }
                break;
            default:
                debug_error(DEBUG_ARP, "unknown arp state");
                display_arp_entry(entry);
                break;
        }
    }
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
    err = net_timer_add(&cache_timer, "arp-timer", arp_cache_tmo, (void *)0, ARP_TIMER_TMO * 1000, NET_TIMER_RELOAD);
    if (err < 0)
    {
        debug_error(DEBUG_ARP, "create timer error: %d", err);
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
    arp_pkt_display(arp_packet);

    ipaddr_t target_ip;
    ipaddr_from_buf(&target_ip, arp_packet->target_paddr);
    if (ipaddr_is_equal(&netif->ipaddr, &target_ip))
    {
        cache_insert(netif, arp_packet->sender_paddr, arp_packet->sender_hwaddr, 1);

        if (x_ntohs(arp_packet->opcode) == ARP_REQUEST)
        {
            debug_info(DEBUG_ARP, "arp request, send reply");
            return arp_make_reply(netif, buf);
        }
    }
    else
    {
        //handle gratuitous arp request
        cache_insert(netif, arp_packet->sender_paddr, arp_packet->sender_hwaddr, 0);
    }

    pktbuf_free(buf);
    return NET_ERR_OK;
}

net_err_t arp_resolve(netif_t * netif, const ipaddr_t * ipaddr, pktbuf_t * buf)
{
    uint8_t ip_buf[IPV4_ADDR_SIZE];
    ipaddr_to_buf(ipaddr, ip_buf);

    arp_entry_t * entry = cache_find((uint8_t *)ip_buf);
    if (entry)
    {
        debug_info(DEBUG_ARP, "find an arp entry");

        if (entry->state == NET_ARP_RESOLVED)
        {
            return ether_raw_out(netif, NET_PROTOCOL_IPV4, entry->hwaddr, buf);
        }

        if (list_count(&entry->buf_list) < ARP_MAX_PKT_WAIT)
        {
            list_insert_last(&entry->buf_list, &buf->node);
            return NET_ERR_OK;
        }
        else
        {
            debug_warn(DEBUG_ARP, "too many pkt");
            return NET_ERR_FULL;
        }
    }
    else
    {
        debug_info(DEBUG_ARP, "make arp request");
        entry = cache_alloc(1);
        if (entry == (arp_entry_t *)0)
        {
            debug_error(DEBUG_ARP, "alloc arp failed");
            return NET_ERR_NONE;
        }
        cache_entry_set(entry, empty_hwaddr, (uint8_t *) ip_buf, netif, NET_ARP_WAITING);
        list_insert_first(&cache_list, &entry->node);
        list_insert_last(&entry->buf_list, &buf->node);
        display_arp_tbl();

        return arp_make_request(netif, ipaddr);
    }
}