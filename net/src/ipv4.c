//
// Created by wj on 2024/4/19.
//

#include "ipv4.h"
#include "debug.h"
#include "tools.h"

net_err_t ipv4_init()
{
    debug_info(DEBUG_IP, "init ipv4");

    return NET_ERR_OK;
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

    return NET_ERR_OK;
}

static void iphdr_ntohs(ipv4_pkt_t * pkt)
{
    pkt->hdr.total_len = x_ntohs(pkt->hdr.total_len);
    pkt->hdr.id = x_ntohs(pkt->hdr.id);
    pkt->hdr.frag_all = x_ntohs(pkt->hdr.frag_all);
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

    pktbuf_free(buf);
    return NET_ERR_OK;
}
