//
// Created by wj on 2024/4/8.
//

#include "ether.h"
#include "netif.h"
#include "pktbuf.h"
#include "debug.h"
#include "tools.h"
#include "protocol.h"
#include "arp.h"
#include "ipv4.h"

static net_err_t is_pkt_ok(ether_pkt_t * pkt, int size)
{
    if (size > (sizeof(ether_hdr_t) + ETHER_MTU))
    {
        return NET_ERR_SIZE;
    }
    if (size < sizeof(ether_hdr_t))
    {
        return NET_ERR_SIZE;
    }
    return NET_ERR_OK;
}

#if DEBUG_DISP_ENABLED(DEBUG_ETHER)
static void display_ether_pkt(char * msg, ether_pkt_t * pkt, int total_size)
{
    ether_hdr_t * hdr = &pkt->hdr;
    plat_printf("------------------ %s ------------------\n", msg);
    plat_printf("\t len: %d bytes \n", total_size);
    debug_dump_hwaddr("\t dest: ", (uint8_t *)hdr->dest, ETHER_HWA_SIZE);
    debug_dump_hwaddr("\t src: ", (uint8_t *)hdr->src, ETHER_HWA_SIZE);
    plat_printf("\t type: %04x \n", x_ntohs(hdr->protocol));
    switch (x_ntohs(hdr->protocol)) {
        case NET_PROTOCOL_ARP:
            plat_printf("arp\n");
            break;
        case NET_PROTOCOL_IPV4:
            plat_printf("ipv4\n");
            break;
        default:
            plat_printf("unknown protocol.\n");
    }
    plat_printf("\n");
}
#else
#define display_ether_pkt(msg, pkt, total_size)
#endif

net_err_t ether_open(struct netif_t * netif)
{
    return arp_make_gratuitous(netif);
}

void ether_close(struct netif_t * netif)
{
    arp_clear(netif);
}

net_err_t ether_in(struct netif_t * netif, pktbuf_t * buf)
{
    debug_info(DEBUG_ETHER, "ether in");

    pktbuf_set_cont(buf, sizeof(ether_hdr_t));
    ether_pkt_t * pkt = (ether_pkt_t*) pktbuf_data(buf);
    net_err_t err;
    if ((err = is_pkt_ok(pkt, buf->total_size)) < 0)
    {
        debug_warn(DEBUG_ETHER, "ether pkt error");
        return err;
    }
    display_ether_pkt("ether in", pkt, buf->total_size);
    switch (x_ntohs(pkt->hdr.protocol)) {
        case NET_PROTOCOL_ARP:
            err = pktbuf_remove_header(buf, sizeof(ether_hdr_t));
            if (err < 0)
            {
                debug_error(DEBUG_ETHER, "remove header failed");
                return NET_ERR_SIZE;
            }
            return arp_in(netif, buf);
            break;
        case NET_PROTOCOL_IPV4:
            arp_update_from_ipbuf(netif, buf);
            err = pktbuf_remove_header(buf, sizeof(ether_hdr_t));
            if (err < 0)
            {
                debug_error(DEBUG_ETHER, "remove header failed");
                return NET_ERR_SIZE;
            }
            return ipv4_in(netif, buf);
            break;
        default:
            debug_warn(DEBUG_ETHER, "unknown packet");
            return NET_ERR_NOT_SUPPORT;
    }
    pktbuf_free(buf);
    return NET_ERR_OK;
}

net_err_t ether_out(struct netif_t * netif, ipaddr_t * dest, pktbuf_t * buf)
{
    if(ipaddr_is_equal(&netif->ipaddr, dest))
    {
        return ether_raw_out(netif, NET_PROTOCOL_IPV4, netif->hwaddr.addr, buf);
    }
    const uint8_t * hwaddr = arp_find(netif, dest);
    if (hwaddr)
    {
        return ether_raw_out(netif, NET_PROTOCOL_IPV4, hwaddr, buf);
    }
    else
    {
        return arp_resolve(netif, dest, buf);
    }
}

net_err_t ether_init()
{
    static const link_layer_t link_layer = {
            .type = NETIF_TYPE_ETHER,
            .open = ether_open,
            .close = ether_close,
            .in = ether_in,
            .out = ether_out
    };

    debug_info(DEBUG_ETHER, "init ether");
    net_err_t err = netif_register_layer(NETIF_TYPE_ETHER, &link_layer);
    if (err < 0)
    {
        debug_error(DEBUG_ETHER, "register ether error");
        return err;
    }

    return NET_ERR_OK;
}

const uint8_t * ether_broadcast_addr(void)
{
    //broadcast addr
    static const uint8_t broadcast[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    return broadcast;
}

net_err_t ether_raw_out(netif_t * netif, uint16_t protocol, const uint8_t * dest, pktbuf_t * buf)
{
    net_err_t err;
    int size = pktbuf_total(buf);
    if (size < ETHER_DATA_MIN)
    {
        debug_info(DEBUG_ETHER, "resize from %d to %d", size, ETHER_DATA_MIN);
        err = pktbuf_resize(buf, ETHER_DATA_MIN);
        if (err < 0)
        {
            debug_error(DEBUG_ETHER, "resize failed");
            return err;
        }
        pktbuf_reset_access(buf);
        pktbuf_seek(buf, size);
        pktbuf_fill(buf, 0, ETHER_DATA_MIN - size);
        size = ETHER_DATA_MIN;
    }

    err = pktbuf_add_header(buf, sizeof(ether_hdr_t), 1);
    if (err < 0)
    {
        debug_error(DEBUG_ETHER, "add header failed: %d", err);
        return NET_ERR_SIZE;
    }
    ether_pkt_t * pkt = (ether_pkt_t *) pktbuf_data(buf);
    plat_memcpy(pkt->hdr.dest, dest, ETHER_HWA_SIZE);
    plat_memcpy(pkt->hdr.src, netif->hwaddr.addr, ETHER_HWA_SIZE);
    pkt->hdr.protocol = x_htons(protocol);

    display_ether_pkt("ether out", pkt, size);
    if (plat_memcpy(netif->hwaddr.addr, dest, ETHER_HWA_SIZE) == 0)
    {
        return netif_put_in(netif, buf, -1);
    }
    else
    {
        err = netif_put_out(netif, buf, -1);
        if (err < 0)
        {
            debug_error(DEBUG_ETHER, "put pkt out failed: %d", err);
            return err;
        }
        return netif->ops->xmit(netif);
    }
}


