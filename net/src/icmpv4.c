//
// Created by wj on 2024/4/24.
//

#include "icmpv4.h"
#include "debug.h"
#include "ipv4.h"

net_err_t icmpv4_init()
{
    debug_info(DEBUG_ICMP, "icmp init");
    return NET_ERR_OK;
}

static net_err_t is_pkt_ok(icmp_pkt_t * icmp_pkt, int total_size, pktbuf_t * buf)
{
    if (total_size <= sizeof(ipv4_pkt_t))
    {
        debug_warn(DEBUG_ICMP, "size error");
        return NET_ERR_SIZE;
    }
    pktbuf_reset_access(buf);
    uint16_t checksum = pktbuf_checksum16(buf, total_size, 0, 1);
    if (checksum != 0)
    {
        debug_warn(DEBUG_ICMP, "bad checksum");
        return NET_ERR_BROKEN;
    }
    return NET_ERR_OK;
}

net_err_t icmpv4_in(ipaddr_t * src_ip, ipaddr_t * netif_ip, pktbuf_t * buf)
{
    debug_info(DEBUG_ICMP, "icmpv4 in");
    ipv4_pkt_t * ip_pkt = (ipv4_pkt_t *) pktbuf_data(buf);
    int iphdr_size = ipv4_hdr_size(ip_pkt);

    net_err_t err = pktbuf_set_cont(buf, iphdr_size + sizeof(icmpv4_hdr_t));
    if (err < 0)
    {
        debug_error(DEBUG_IP, "set icmp cont failed");
        return err;
    }

    ip_pkt = (ipv4_pkt_t *) pktbuf_data(buf);

    err = pktbuf_remove_header(buf, iphdr_size);
    if (err < 0)
    {
        debug_error(DEBUG_IP, "remove ip header failed");
        return err;
    }
    icmp_pkt_t * icmp_pkt = (icmp_pkt_t *) pktbuf_data(buf);
    if ((err = is_pkt_ok(icmp_pkt, buf->total_size, buf)) < 0)
    {
        debug_warn(DEBUG_ICMP, "icmp pkt error");
        return err;
    }
    return NET_ERR_OK;
}
