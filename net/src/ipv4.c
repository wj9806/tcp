//
// Created by wj on 2024/4/19.
//

#include "ipv4.h"
#include "debug.h"
#include "tools.h"
#include "protocol.h"
#include "icmpv4.h"
#include "mblock.h"
#include "timer.h"
#include "raw.h"
#include "udp.h"

static uint16_t packet_id = 0;
static ip_frag_t frag_array[IP_FRAGS_MAX_NR];

static mblock_t frag_mblock;
static list_t frag_list;
static net_timer_t frag_timer;

static list_t rt_list;
static rentry_t rt_table[IP_RTTABLE_SIZE];
static mblock_t rt_block;

#if DEBUG_DISP_ENABLED(DEBUG_IP)
void rt_list_display()
{
    plat_printf("------------rt table---------------\n");

    node_t * node;
    list_for_each(node, &rt_list)
    {
        rentry_t * entry = (rentry_t*) list_node_parent(node, rentry_t, node);
        debug_dump_ip("net: ", &entry->net);
        debug_dump_ip("  mask: ", &entry->mask);
        debug_dump_ip("  next_hop: ", &entry->next_hop);
        plat_printf("  netif: %s\n", entry->netif->name);
    }
}
#else
#define rt_list_display()
#endif

static int get_data_size(ipv4_pkt_t * pkt)
{
    return pkt->hdr.total_len - ipv4_hdr_size(pkt);
}

static uint16_t get_frag_start(ipv4_pkt_t * pkt)
{
    return pkt->hdr.frag_offset * 8;
}

static uint16_t get_frag_end(ipv4_pkt_t * pkt)
{
    return get_frag_start(pkt) + get_data_size(pkt);
}

#if DEBUG_DISP_ENABLED(DEBUG_IP)
static void display_ip_pkt(ipv4_pkt_t * pkt)
{
    ipv4_hdr_t * ip_hdr = &pkt->hdr;
    plat_printf("---------------ip pkt---------------\n");

    plat_printf("       version: %d\n", ip_hdr->version);
    plat_printf("       header len: %d\n", ipv4_hdr_size(pkt));
    plat_printf("       total len: %d\n", ip_hdr->total_len);
    plat_printf("       id: %d\n", ip_hdr->id);
    plat_printf("       ttl: %d\n", ip_hdr->ttl);
    plat_printf("       frag offset: %d\n", ip_hdr->frag_offset);
    plat_printf("       frag more: %d\n", ip_hdr->more);
    plat_printf("       protocol: %d\n", ip_hdr->protocol);
    plat_printf("       checksum: %d\n", ip_hdr->header_checksum);
    debug_dump_ip_buf("       src ip : ", ip_hdr->src_ip);
    debug_dump_ip_buf("       dest ip : ", ip_hdr->dest_ip);
    plat_printf("\n");
}

static void display_ip_frags()
{
    plat_printf("---------------ip frags---------------\n");
    node_t * node;
    int index = 0;
    list_for_each(node, &frag_list)
    {
        ip_frag_t * frag = list_node_parent(node, ip_frag_t, node);
        plat_printf("[%d]: \n", index++);
        debug_dump_ip("   ip ", &frag->ip);
        plat_printf("   id: %d\n", frag->id);
        plat_printf("   tmo: %d\n", frag->tmo);
        plat_printf("   bufs: %d\n", list_count(&frag->buf_list));
        plat_printf("   bufs:\n");
        node_t * pnode;
        int pindex = 0;
        list_for_each(pnode, &frag->buf_list)
        {
            pktbuf_t * buf = list_node_parent(pnode, pktbuf_t, node);
            ipv4_pkt_t * pkt = (ipv4_pkt_t *) pktbuf_data(buf);
            plat_printf("   %d:[%d-%d], ", pindex++, get_frag_start(pkt), get_frag_end(pkt) - 1);
        }
        plat_printf("\n");
    }
}
#else
#define display_ip_pkt(pkt)
#define display_ip_frags()
#endif

static void frag_free_buf_list(ip_frag_t * frag)
{
    node_t * node;
    while ((node = list_remove_first(&frag->buf_list)))
    {
        pktbuf_t * buf = list_node_parent(node, pktbuf_t, node);
        pktbuf_free(buf);
    }
}

static void frag_free(ip_frag_t * frag)
{
    frag_free_buf_list(frag);
    list_remove(&frag_list, &frag->node);
    mblock_free(&frag_mblock, frag);
}

/**
 * ip frag packet timeout scan function
 */
static void frag_tmo(struct net_timer_t * timer, void * arg)
{
    node_t * curr, * next;
    for (curr = list_first(&frag_list); curr; curr = next)
    {
        next = list_node_next(curr);
        ip_frag_t * frag = list_node_parent(curr, ip_frag_t, node);
        if (--frag->tmo <= 0)
        {
            frag_free(frag);
        }
    }
}

static net_err_t frag_init()
{
    list_init(&frag_list);
    net_err_t err = mblock_init(&frag_mblock, frag_array, sizeof(ip_frag_t), IP_FRAGS_MAX_NR, LOCKER_NONE);
    if (err < 0)
    {
        debug_error(DEBUG_IP, "mblock init failed");
        return err;
    }

    err = net_timer_add(&frag_timer, "frag-timer", frag_tmo, (void *)0, IP_FRAG_SCAN_PERIOD * 1000, NET_TIMER_RELOAD);
    if (err < 0)
    {
        debug_error(DEBUG_IP, "create frag timer failed");
        return err;
    }
    return NET_ERR_OK;
}

/**
 * init the route table
 */
static void rt_init()
{
    list_init(&rt_list);
    mblock_init(&rt_block, rt_table, sizeof(rentry_t), IP_RTTABLE_SIZE, LOCKER_NONE);
}

net_err_t ipv4_init()
{
    debug_info(DEBUG_IP, "init ipv4");

    net_err_t err = frag_init();
    if (err < 0)
    {
        debug_error(DEBUG_IP, "frag init failed");
        return err;
    }
    rt_init();
    return NET_ERR_OK;
}

//alloc frag
static ip_frag_t * frag_alloc()
{
    ip_frag_t * frag = mblock_alloc(&frag_mblock, -1);
    if (!frag)
    {
        node_t * node = list_remove_last(&frag_list);
        frag = list_node_parent(node, ip_frag_t, node);
        if (frag)
        {
            frag_free_buf_list(frag);
        }
    }
    return frag;
}

static ip_frag_t * frag_find(ipaddr_t * ip, uint16_t id)
{
    node_t * curr;

    list_for_each(curr, &frag_list)
    {
        ip_frag_t * frag = list_node_parent(curr, ip_frag_t, node);
        if (ipaddr_is_equal(ip, &frag->ip) && (id == frag->id))
        {
            list_remove(&frag_list, curr);
            list_insert_first(&frag_list, curr);
            return frag;
        }
    }
    return (ip_frag_t *) 0;
}

static net_err_t is_pkt_ok(ipv4_pkt_t * pkt, int size, netif_t * netif)
{
    if (pkt->hdr.version != NET_VERSION_IPV4)
    {
        debug_warn(DEBUG_IP, "invalid ip version");
        return NET_ERR_NOT_SUPPORT;
    }

    int hdr_len = ipv4_hdr_size(pkt);
    if (hdr_len < sizeof(ipv4_hdr_t ))
    {
        debug_warn(DEBUG_IP, "ipv4 header error");
        return NET_ERR_SIZE;
    }

    int total_size = x_ntohs(pkt->hdr.total_len);
    if ((total_size < sizeof(ipv4_hdr_t )) || (size < total_size))
    {
        debug_warn(DEBUG_IP, "ipv4 size error");
        return NET_ERR_SIZE;
    }
    if (pkt->hdr.header_checksum)
    {
        uint16_t ch = checksum16(0, pkt, hdr_len, 0, 1);
        if (ch != 0)
        {
            debug_warn(DEBUG_IP, "bad checksum");
            return NET_ERR_BROKEN;
        }
    }
    return NET_ERR_OK;
}

static void iphdr_ntohs(ipv4_pkt_t * pkt)
{
    pkt->hdr.total_len = x_ntohs(pkt->hdr.total_len);
    pkt->hdr.id = x_ntohs(pkt->hdr.id);
    pkt->hdr.frag_all = x_ntohs(pkt->hdr.frag_all);
}

static void iphdr_htons(ipv4_pkt_t * pkt)
{
    pkt->hdr.total_len = x_htons(pkt->hdr.total_len);
    pkt->hdr.id = x_htons(pkt->hdr.id);
    pkt->hdr.frag_all = x_htons(pkt->hdr.frag_all);
}

static void frag_add(ip_frag_t * frag, ipaddr_t * ip, uint16_t id)
{
    ipaddr_copy(&frag->ip, ip);
    frag->tmo = IP_FRAG_TMO / IP_FRAG_SCAN_PERIOD;
    frag->id = id;
    node_init(&frag->node);
    list_init(&frag->buf_list);
    list_insert_first(&frag_list, &frag->node);
}

static net_err_t frag_insert(ip_frag_t * frag, pktbuf_t * buf, ipv4_pkt_t * pkt)
{
    if (list_count(&frag->buf_list) >= IP_FRAG_MAX_BUF_NR)
    {
        debug_error(DEBUG_IP, "buf_list size >= %d", IP_FRAG_MAX_BUF_NR);
        frag_free(frag);
        return NET_ERR_FULL;
    }
    node_t * node;
    list_for_each(node, &frag->buf_list)
    {
        pktbuf_t * curr_buf = list_node_parent(node, pktbuf_t, node);
        ipv4_pkt_t * curr_pkt = (ipv4_pkt_t *) pktbuf_data(curr_buf);
        uint16_t curr_start = get_frag_start(curr_pkt);
        if (get_frag_start(pkt) == curr_start)
        {
            return NET_ERR_EXIST;
        }
        else if(get_frag_end(pkt) <= curr_start)
        {
            node_t * pre = list_node_pre(node);
            if (pre)
            {
                list_insert_after(&frag->buf_list, pre, &buf->node);
            }
            else
            {
                list_insert_first(&frag->buf_list, &buf->node);
            }
            return NET_ERR_OK;
        }
    }
    list_insert_last(&frag->buf_list, &buf->node);
    return NET_ERR_OK;
}

/**
 * Determine whether all fragments have arrived
 */
static int frag_is_all_arrived(ip_frag_t * frag)
{
    int offset = 0;
    node_t * node;
    ipv4_pkt_t * pkt = (ipv4_pkt_t *)0;
    list_for_each(node, &frag->buf_list)
    {
        pktbuf_t * buf = list_node_parent(node, pktbuf_t, node);
        pkt = (ipv4_pkt_t *) pktbuf_data(buf);

        int curr_offset = get_frag_start(pkt);
        if (curr_offset != offset)
        {
            return 0;
        }

        offset += get_data_size(pkt);
    }

    return pkt ? !pkt->hdr.more : 0;
}

/**
 * merge fragments
 */
static pktbuf_t * frag_merge(ip_frag_t * frag)
{
    pktbuf_t * target = (pktbuf_t *)0;
    node_t * node;
    while ((node = list_remove_first(&frag->buf_list)))
    {
        pktbuf_t * curr = list_node_parent(node, pktbuf_t, node);

        if (!target)
        {
            target = curr;
            continue;
        }
        ipv4_pkt_t * pkt = (ipv4_pkt_t *) pktbuf_data(curr);
        net_err_t err = pktbuf_remove_header(curr, ipv4_hdr_size(pkt));
        if (err < 0)
        {
            debug_error(DEBUG_IP, "remove ip header failed");
            pktbuf_free(curr);
            goto free_and_return;
        }
        err = pktbuf_merge(target, curr);
        if (err < 0)
        {
            debug_error(DEBUG_IP, "join ip frag failed");
            pktbuf_free(curr);
            goto free_and_return;
        }
    }
    frag_free(frag);
    return target;
    free_and_return:
    if (target)
    {
        pktbuf_free(target);
    }
    frag_free(frag);
    return (pktbuf_t*)0;
}

static net_err_t ip_normal_in(netif_t * netif, pktbuf_t * buf, ipaddr_t * src_ip, ipaddr_t * dest_ip);

static net_err_t ip_frag_in(netif_t * netif, pktbuf_t * buf, ipaddr_t * src_ip, ipaddr_t * dest_ip)
{
    ipv4_pkt_t * curr = (ipv4_pkt_t *) pktbuf_data(buf);
    ip_frag_t * frag = frag_find(src_ip, curr->hdr.id);
    if (!frag)
    {
        frag = frag_alloc();
        frag_add(frag, src_ip, curr->hdr.id);
    }
    net_err_t err = frag_insert(frag, buf, curr);
    if (err < 0)
    {
        debug_warn(DEBUG_IP, "frag insert failed");
        return err;
    }
    if (frag_is_all_arrived(frag))
    {
        pktbuf_t * full_buf = frag_merge(frag);
        if (!full_buf)
        {
            debug_error(DEBUG_IP, "merge ip bufs failed");
            display_ip_frags();
            return NET_ERR_OK;
        }

        err = ip_normal_in(netif, full_buf, src_ip, dest_ip);
        if (err < 0)
        {
            debug_warn(DEBUG_IP, "ip frag in failed: %d", err);
            pktbuf_free(full_buf);
            return NET_ERR_OK;
        }
    }
    display_ip_frags();
    return NET_ERR_OK;
}

static net_err_t ip_normal_in(netif_t * netif, pktbuf_t * buf, ipaddr_t * src_ip, ipaddr_t * dest_ip)
{
    ipv4_pkt_t * pkt = (ipv4_pkt_t *)pktbuf_data(buf);
    display_ip_pkt(pkt);
    switch (pkt->hdr.protocol) {
        case NET_PROTOCOL_ICMPv4:
        {
            net_err_t err = icmpv4_in(src_ip, &netif->ipaddr, buf);
            if (err < 0) {
                debug_warn(DEBUG_IP, "icmp in failed");
                return err;
            }
            break;
        }
        case NET_PROTOCOL_UDP:
        {
            net_err_t err = udp_in(buf, src_ip, dest_ip);
            if (err < 0)
            {
                debug_warn(DEBUG_IP, "udp in error");

                if (err == NET_ERR_UNREACHABLE)
                {
                    iphdr_htons(pkt);
                    icmpv4_out_unreachable(dest_ip, &netif->ipaddr, ICMPv4_UNREACHABLE_PORT, buf);
                }
                return err;
            }
            return NET_ERR_OK;
        }
        case NET_PROTOCOL_TCP:
            break;
        default:
            debug_warn(DEBUG_IP, "unknown protocol");
            net_err_t err = raw_in(buf);
            if (err < 0)
            {
                debug_error(DEBUG_IP, "raw in failed:%d", err);
                return err;
            }
            break;
    }
    return NET_ERR_UNREACHABLE;
}

net_err_t ipv4_in(netif_t * netif, pktbuf_t * buf)
{
    debug_info(DEBUG_IP, "ipv4 in");
    net_err_t err = pktbuf_set_cont(buf, sizeof(ipv4_hdr_t));
    if (err < 0)
    {
        debug_error(DEBUG_IP, "ajust header failed, err = %d", err);
        return err;
    }
    ipv4_pkt_t * pkt = (ipv4_pkt_t *) pktbuf_data(buf);
    err = is_pkt_ok(pkt, buf->total_size, netif);
    if (err != NET_ERR_OK)
    {
        debug_warn(DEBUG_IP, "ip packet check failed, err = %d", err);
        return err;
    }

    iphdr_ntohs(pkt);
    err = pktbuf_resize(buf, pkt->hdr.total_len);
    if (err < 0)
    {
        debug_warn(DEBUG_IP, "resize ip packet failed, err = %d", err);
        return err;
    }

    ipaddr_t dest_ip, src_ip;
    ipaddr_from_buf(&dest_ip, pkt->hdr.dest_ip);
    ipaddr_from_buf(&src_ip, pkt->hdr.src_ip);

    if (!ipaddr_is_match(&dest_ip, &netif->ipaddr, &netif->netmask))
    {
        debug_warn(DEBUG_IP, "ipaddr not match");
        return NET_ERR_UNREACHABLE;
    }
    if (pkt->hdr.frag_offset || pkt->hdr.more)
    {
        err = ip_frag_in(netif, buf, &src_ip, &dest_ip);
    }
    else
    {
        err = ip_normal_in(netif, buf, &src_ip, &dest_ip);
    }
    return NET_ERR_OK;
}

net_err_t ip_frag_out(uint8_t protocol, ipaddr_t * dest, ipaddr_t * src, pktbuf_t * buf, ipaddr_t * next_hop, netif_t * netif)
{
    debug_info(DEBUG_IP, "frag send ip pkt");
    pktbuf_reset_access(buf);

    int offset = 0;
    int total = buf->total_size;

    while (total)
    {
        int curr_size = total;
        if (curr_size + sizeof(ipv4_hdr_t)> netif->mtu)
        {
            curr_size = (int)(netif->mtu - sizeof(ipv4_hdr_t));
        }

        pktbuf_t * dest_buf = pktbuf_alloc((int) (curr_size + sizeof(ipv4_hdr_t)));
        if (!dest_buf)
        {
            debug_error(DEBUG_IP, "alloc buf failed");
            return NET_ERR_NONE;
        }
        ipv4_pkt_t * pkt = (ipv4_pkt_t *) pktbuf_data(dest_buf);
        pkt->hdr.shdr_all = 0;
        pkt->hdr.version = NET_VERSION_IPV4;
        ipv4_set_hdr_size(pkt, sizeof(ipv4_hdr_t));
        pkt->hdr.total_len = dest_buf->total_size;
        pkt->hdr.id = packet_id;
        pkt->hdr.frag_all = 0;
        pkt->hdr.ttl = NET_IP_DEFAULT_TTL;
        pkt->hdr.protocol = protocol;
        pkt->hdr.header_checksum = 0;
        if (!src || ipaddr_is_any(src))
        {
            ipaddr_to_buf(&netif->ipaddr, pkt->hdr.src_ip);
        }
        else
        {
            ipaddr_to_buf(src, pkt->hdr.src_ip);
        }
        ipaddr_to_buf(dest, pkt->hdr.dest_ip);
        pkt->hdr.frag_offset = offset >> 3;
        pkt->hdr.more = total > curr_size;
        pktbuf_seek(dest_buf, sizeof(ipv4_hdr_t));
        net_err_t err = pktbuf_copy(dest_buf, buf, curr_size);
        if (err < 0)
        {
            debug_error(DEBUG_IP, "frag buf copy failed");
            pktbuf_free(dest_buf);
            return err;
        }

        pktbuf_remove_header(buf, curr_size);
        pktbuf_reset_access(buf);

        iphdr_htons(pkt);
        pktbuf_reset_access(dest_buf);
        pkt->hdr.header_checksum = pktbuf_checksum16(dest_buf, ipv4_hdr_size(pkt), 0, 1);

        display_ip_pkt(pkt);

        err = netif_out(netif, next_hop, dest_buf);
        if (err < 0)
        {
            debug_warn(DEBUG_IP, "send ip frag failed");
            pktbuf_free(dest_buf);
            return err;
        }
        total -= curr_size;
        offset += curr_size;
    }
    packet_id++;
    pktbuf_free(buf);
    return NET_ERR_OK;
}


net_err_t ipv4_out(uint8_t protocol, ipaddr_t * dest, ipaddr_t * src, pktbuf_t * buf)
{
    debug_info(DEBUG_IP, "send ip packet");

    rentry_t * rt = rt_find(dest);
    if (!rt)
    {
        debug_error(DEBUG_IP, "send failed, no route");
        return NET_ERR_UNREACHABLE;
    }
    ipaddr_t next_hop;
    if (ipaddr_is_any(&rt->next_hop))
    {
        ipaddr_copy(&next_hop, dest);
    }
    else
    {
        ipaddr_copy(&next_hop, &rt->next_hop);
    }

    netif_t * netif = rt->netif;

    if (netif->mtu && ((buf->total_size + sizeof(ipv4_hdr_t)) > netif->mtu))
    {
        net_err_t err = ip_frag_out(protocol, dest, src, buf, &next_hop, netif);
        if (err < 0)
        {
            debug_warn(DEBUG_IP, "send ip frag failed");
            return err;
        }
        return NET_ERR_OK;
    }

    net_err_t err = pktbuf_add_header(buf, sizeof(ipv4_hdr_t), 1);
    if (err < 0)
    {
        debug_error(DEBUG_IP, "add ip header error");
        return NET_ERR_SIZE;
    }

    ipv4_pkt_t * pkt = (ipv4_pkt_t *) pktbuf_data(buf);
    pkt->hdr.shdr_all = 0;
    pkt->hdr.version = NET_VERSION_IPV4;
    ipv4_set_hdr_size(pkt, sizeof(ipv4_hdr_t));
    pkt->hdr.total_len = buf->total_size;
    pkt->hdr.id = packet_id++;
    pkt->hdr.frag_all = 0;
    pkt->hdr.ttl = NET_IP_DEFAULT_TTL;
    pkt->hdr.protocol = protocol;
    pkt->hdr.header_checksum = 0;
    if (!src || ipaddr_is_any(src))
    {
        ipaddr_to_buf(&netif->ipaddr, pkt->hdr.src_ip);
    }
    else
    {
        ipaddr_to_buf(src, pkt->hdr.src_ip);
    }
    ipaddr_to_buf(dest, pkt->hdr.dest_ip);

    iphdr_htons(pkt);
    pktbuf_reset_access(buf);
    pkt->hdr.header_checksum = pktbuf_checksum16(buf, ipv4_hdr_size(pkt), 0, 1);
    display_ip_pkt(pkt);
    err = netif_out(netif, &next_hop, buf);
    if (err < 0)
    {
        debug_warn(DEBUG_IP, "send ip packet failed");
        return err;
    }
    return NET_ERR_OK;
}

void rt_add(ipaddr_t * net, ipaddr_t * mask, ipaddr_t * next_hop, netif_t * netif)
{
    rentry_t * entry = mblock_alloc(&rt_block, -1);
    if (!entry)
    {
        debug_warn(DEBUG_IP, "alloc rt entry failed");
        return;
    }

    ipaddr_copy(&entry->net, net);
    ipaddr_copy(&entry->mask, mask);
    ipaddr_copy(&entry->next_hop, next_hop);
    entry->netif = netif;
    entry->mask_1_cnt = ipaddr_1_cnt(mask);
    list_insert_last(&rt_list, &entry->node);
    rt_list_display();
}

void rt_remove(ipaddr_t * net, ipaddr_t * mask)
{
    node_t * node;
    list_for_each(node, &rt_list)
    {
        rentry_t * entry = list_node_parent(node, rentry_t, node);
        if (ipaddr_is_equal(&entry->net, net) && ipaddr_is_equal(&entry->mask, mask))
        {
            list_remove(&rt_list, node);
            return;
        }
    }
    rt_list_display();
}

rentry_t * rt_find(ipaddr_t * ip)
{
    rentry_t * e = (rentry_t *)0;
    node_t * node;
    list_for_each(node, &rt_list)
    {
        rentry_t * entry = list_node_parent(node, rentry_t, node);
        ipaddr_t net = ipaddr_get_net(ip, &entry->mask);
        if (!ipaddr_is_equal(&net, &entry->net))
        {
            continue;
        }
        if (!e || (e->mask_1_cnt < entry->mask_1_cnt))
        {
            e = entry;
        }
    }
    return e;
}