//
// Created by wj on 2024/4/24.
//

#include "icmpv4.h"
#include "debug.h"
#include "ipv4.h"
#include "protocol.h"

#if DEBUG_DISP_ENABLED(DEBUG_ICMP)
static void display_icmp_pkt(char * title, icmp_pkt_t * pkt)
{
    plat_printf("--------------- %s ------------", title);
    plat_printf("       type: %d\n", pkt->hdr.type);
    plat_printf("       code: %d\n", pkt->hdr.code);
    plat_printf("       checksum: %d\n", pkt->hdr.checksum);
}
#else
#define display_icmp_pkt(title, pkt);
#endif

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

static net_err_t icmpv4_out(ipaddr_t * dest, ipaddr_t * src, pktbuf_t * buf)
{
    icmp_pkt_t * pkt = (icmp_pkt_t *) pktbuf_data(buf);
    pktbuf_reset_access(buf);
    pkt->hdr.checksum = pktbuf_checksum16(buf, buf->total_size, 0, 1);
    display_icmp_pkt("icmp out", pkt);
    return ipv4_out(NET_PROTOCOL_ICMPv4, dest, src, buf);
}

static net_err_t icmpv4_echo_reply(ipaddr_t * dest, ipaddr_t * src, pktbuf_t * buf)
{
    icmp_pkt_t * pkt = (icmp_pkt_t *) pktbuf_data(buf);
    pkt->hdr.type = ICMPv4_ECHO_REPLY;
    pkt->hdr.checksum = 0;
    return icmpv4_out(dest, src, buf);
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
    display_icmp_pkt("icmp in", icmp_pkt);
    switch (icmp_pkt->hdr.type) {
        case ICMPv4_ECHO_REQUEST:
            return icmpv4_echo_reply(src_ip, netif_ip, buf);
            break;
        default:
            pktbuf_free(buf);
            break;
    }
    return NET_ERR_OK;
}

net_err_t icmpv4_out_unreachable(ipaddr_t * dest_ip, ipaddr_t * src, uint8_t code, pktbuf_t * buf)
{
    int copy_size = ipv4_hdr_size((ipv4_pkt_t *) pktbuf_data(buf)) + 576;
    if (copy_size > buf->total_size)
    {
        copy_size = buf->total_size;
    }

    pktbuf_t * new_buf = pktbuf_alloc(copy_size + sizeof(icmpv4_hdr_t) + 4);
    if (!new_buf)
    {
        debug_warn(DEBUG_ICMP, "alloc buf failed");
        return NET_ERR_NONE;
    }

    icmp_pkt_t * pkt = (icmp_pkt_t *) pktbuf_data(new_buf);
    pkt->hdr.type = ICMPv4_UNREACHABLE;
    pkt->hdr.code = code;
    pkt->hdr.checksum = 0;
    pkt->reverse = 0;
    pktbuf_reset_access(buf);
    pktbuf_seek(new_buf, sizeof(icmpv4_hdr_t) + 4);
    net_err_t err = pktbuf_copy(new_buf, buf, copy_size);
    if (err < 0)
    {
        debug_error(DEBUG_ICMP, "copy buf failed");
        pktbuf_free(new_buf);
        return err;
    }
    err = icmpv4_out(dest_ip, src, new_buf);
    if (err < 0)
    {
        debug_error(DEBUG_ICMP, "icmpv4 out failed");
        pktbuf_free(new_buf);
        return err;
    }
    return NET_ERR_OK; 
}