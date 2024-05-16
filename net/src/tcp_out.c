//
// Created by wj on 2024/5/16.
//

#include "tcp_out.h"
#include "tools.h"
#include "protocol.h"
#include "ipv4.h"

static net_err_t send_out(tcp_hdr_t * out, pktbuf_t * buf, ipaddr_t * dest, ipaddr_t * src)
{
    out->sport = x_htons(out->sport);
    out->dport = x_htons(out->dport);
    out->seq = x_htonl(out->seq);
    out->ack = x_htonl(out->ack);
    out->win = x_htons(out->win);
    out->urg_ptr = x_htons(out->urg_ptr);
    out->checksum = 0;
    out->checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_TCP);

    net_err_t err = ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    if (err < 0)
    {
        debug_warn(DEBUG_TCP, "send tcp reset failed");
        pktbuf_free(buf);
        return err;
    }

    return NET_ERR_OK;
}

net_err_t tcp_send_reset(tcp_seg_t * seg)
{
    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf)
    {
        debug_warn(DEBUG_TCP, "alloc pktbuf failed");
        return NET_ERR_NONE;
    }

    tcp_hdr_t * out = (tcp_hdr_t *) pktbuf_data(buf);
    tcp_hdr_t * in = seg->hdr;

    out->sport = in->dport;
    out->dport = in->sport;
    out->flag = 0;
    out->f_rst = 1;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));

    if (in->f_ack)
    {
        out->seq = in->ack;
        out->ack = 0;
        out->f_ack = 0;
    }
    else
    {
        //invalid ack
        out->ack = in->seq + seg->seq_len;
        out->f_ack = 1;
    }

    out->win = out->urg_ptr = 0;

    return send_out(out, buf, &seg->remote_ip, &seg->local_ip);
}
