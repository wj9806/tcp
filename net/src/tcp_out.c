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
    tcp_show_pkt("tcp out", out, buf);
    net_err_t err = ipv4_out(NET_PROTOCOL_TCP, dest, src, buf);
    if (err < 0)
    {
        debug_warn(DEBUG_TCP, "send tcp reset failed: %d", err);
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

/**
 *
 * |_________|____________|______________|
 *          UNA            NXT
 */
static void get_send_info(tcp_t * tcp, int rexmit, int * doff, int * dlen)
{
    if (rexmit)
    {
        *doff = 0;
        *dlen = tcp_buf_cnt(&tcp->snd.buf);
    }
    else
    {
        *doff = (int) (tcp->snd.nxt - tcp->snd.una);
        *dlen = tcp_buf_cnt(&tcp->snd.buf) - *doff;
    }
    *dlen = (*dlen > tcp->mss) ? tcp->mss : *dlen;
}

static int copy_send_data(tcp_t * tcp, pktbuf_t * buf, int doff, int dlen)
{
    if (dlen == 0)
    {
        return 0;
    }

    net_err_t err = pktbuf_resize(buf, (int)(buf->total_size + dlen));
    if (err < 0)
    {
        debug_error(DEBUG_TCP, "pktbuf resize failed");
        return -1;
    }

    int hdr_size = tcp_hdr_size((tcp_hdr_t *) pktbuf_data(buf));
    pktbuf_reset_access(buf);
    pktbuf_seek(buf, hdr_size);

    tcp_buf_read_send(&tcp->snd.buf, doff, buf, dlen);
    return dlen;
}

static void write_sync_option(tcp_t * tcp, pktbuf_t * buf)
{
    int opt_len = sizeof(tcp_opt_mss_t);

    net_err_t err = pktbuf_resize(buf, buf->total_size + opt_len);
    if (err < 0)
    {
        debug_error(DEBUG_TCP, "resize pktbuf error");
        return;
    }

    tcp_opt_mss_t mss;
    mss.kind = TCP_OPT_MSS;
    mss.length = sizeof(tcp_opt_mss_t);
    mss.mss = x_ntohs(tcp->mss);

    pktbuf_reset_access(buf);
    pktbuf_seek(buf, sizeof(tcp_hdr_t));
    pktbuf_write(buf, (uint8_t *)&mss, sizeof(mss));
}

net_err_t tcp_transmit(tcp_t * tcp)
{
    int dlen, doff;
    get_send_info(tcp, 0, &doff, &dlen);
    if (dlen < 0)
    {
        return NET_ERR_OK;
    }

    int seq_len = dlen;
    if (tcp->flags.syn_out)
    {
        seq_len++;
    }
    if (tcp->flags.fin_out)
    {
        seq_len++;
    }
    if (seq_len == 0)
    {
        return NET_ERR_OK;
    }

    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf)
    {
        debug_error(DEBUG_TCP, "no buffer");
        return NET_ERR_OK;
    }

    tcp_hdr_t * hdr = (tcp_hdr_t *) pktbuf_data(buf);
    plat_memset(hdr, 0, sizeof(tcp_hdr_t));
    hdr->sport = tcp->base.local_port;
    hdr->dport = tcp->base.remote_port;
    hdr->seq = tcp->snd.nxt;
    hdr->ack = tcp->rcv.nxt;
    hdr->flag = 0;
    hdr->f_syn = tcp->flags.syn_out;
    hdr->f_ack = tcp->flags.irs_valid;
    hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? tcp->flags.fin_out : 0;
    hdr->win = tcp_rcv_window(tcp);
    hdr->urg_ptr = 0;

    if (hdr->f_syn)
    {
        //write mss option
        write_sync_option(tcp, buf);
    }

    tcp_set_hdr_size(hdr, buf->total_size);

    //Copy the packets that will be sent
    copy_send_data(tcp, buf, doff, dlen);

    tcp->snd.nxt += hdr->f_syn + hdr->f_fin + dlen;
    return send_out(hdr, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

net_err_t tcp_send_syn(tcp_t * tcp)
{
    tcp->flags.syn_out = 1;
    //tcp_transmit(tcp);
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    return NET_ERR_OK;
}

net_err_t tcp_ack_process(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (TCP_SEQ_LE(tcp_hdr->ack, tcp->snd.una))
    {
        return NET_ERR_OK;
    }
    else if(TCP_SEQ_LT(tcp->snd.nxt, tcp_hdr->ack))
    {
        return NET_ERR_UNREACHABLE;
    }

    if (tcp->flags.syn_out)
    {
        tcp->snd.una++;
        tcp->flags.syn_out = 0;
    }

    int acked_cnt =(int) (tcp_hdr->ack - tcp->snd.una);
    int unacked = (int) (tcp->snd.nxt - tcp->snd.una);
    int curr_acked = (acked_cnt > unacked) ? unacked : acked_cnt;
    if (curr_acked > 0) {
        sock_wakeup(&tcp->base, SOCK_WAIT_WRITE, NET_ERR_OK);
        tcp->snd.una += curr_acked;
        curr_acked -= tcp_buf_remove(&tcp->snd.buf, curr_acked);

        if (tcp->flags.fin_out && curr_acked) {
            tcp->flags.fin_out = 0;
        }
    }
    return NET_ERR_OK;
}

net_err_t tcp_send_ack(tcp_t * tcp, tcp_seg_t * seg)
{
    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));

    if (!buf)
    {
        debug_error(DEBUG_TCP, "no buffer");
        return NET_ERR_NONE;
    }

    tcp_hdr_t * hdr = (tcp_hdr_t *) pktbuf_data(buf);
    hdr->sport = tcp->base.local_port;
    hdr->dport = tcp->base.remote_port;
    hdr->seq = tcp->snd.nxt;
    hdr->ack = tcp->rcv.nxt;
    hdr->flag = 0;
    hdr->f_ack = 1;
    hdr->win = tcp_rcv_window(tcp);
    hdr->urg_ptr = 0;
    tcp_set_hdr_size(hdr, sizeof(tcp_hdr_t));
    return send_out(hdr, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

net_err_t tcp_send_fin(tcp_t * tcp)
{
    tcp->flags.fin_out = 1;
    tcp_out_event(tcp, TCP_OEVENT_SEND);
    return NET_ERR_OK;
}

int tcp_write_sndbuf(tcp_t* tcp, const uint8_t* buf, int len)
{
    int free_cnt = tcp_buf_free_cnt(&tcp->snd.buf);
    if (free_cnt <= 0)
    {
        return 0;
    }

    int wr_len = (len > free_cnt) ? free_cnt : len;
    tcp_buf_write_send(&tcp->snd.buf, buf, wr_len);
    return wr_len;
}

net_err_t tcp_send_reset_for_tcp(tcp_t * tcp)
{
    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf)
    {
        debug_warn(DEBUG_TCP, "alloc pktbuf failed");
        return NET_ERR_NONE;
    }

    tcp_hdr_t * out = (tcp_hdr_t *) pktbuf_data(buf);

    out->sport = tcp->base.local_port;
    out->dport = tcp->base.remote_port;
    out->seq = tcp->snd.nxt;
    out->ack = tcp->rcv.nxt;
    out->flag = 0;
    out->f_rst = 1;
    out->f_ack = 1;
    out->win = tcp_rcv_window(tcp);
    out->urg_ptr = 0;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));

    return send_out(out, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

net_err_t tcp_send_keepalive(tcp_t * tcp)
{
    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf)
    {
        debug_warn(DEBUG_TCP, "alloc pktbuf failed");
        return NET_ERR_NONE;
    }

    tcp_hdr_t * out = (tcp_hdr_t *) pktbuf_data(buf);

    out->sport = tcp->base.local_port;
    out->dport = tcp->base.remote_port;
    out->seq = tcp->snd.nxt - 1;
    out->ack = tcp->rcv.nxt;
    out->flag = 0;
    out->f_ack = 1;
    out->win = tcp_rcv_window(tcp);
    out->urg_ptr = 0;
    tcp_set_hdr_size(out, sizeof(tcp_hdr_t));

    return send_out(out, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

const char * tcp_ostate_name(tcp_t * tcp)
{
    static const char * state_name[] = {
            [TCP_OSTATE_IDLE] = "idle",
            [TCP_OSTATE_SENDING] = "sending",
            [TCP_OSTATE_REXMIT] = "resending",

            [TCP_OSTATE_MAX] = "unknown"
    };
    tcp_ostate_t state = tcp->snd.ostate;
    if (state > TCP_OSTATE_MAX)
    {
        state = TCP_OSTATE_MAX;
    }
    return state_name[state];
}

net_err_t tcp_retransmit(tcp_t * tcp)
{
    int dlen, doff;
    get_send_info(tcp, 1, &doff, &dlen);

    if (dlen < 0) {
        return NET_ERR_OK;
    }

    int seq_len = dlen;
    if (tcp->flags.syn_out)
    {
        seq_len++;
    }
    if (tcp->flags.fin_out)
    {
        seq_len++;
    }
    if (seq_len == 0)
    {
        return NET_ERR_OK;
    }

    pktbuf_t * buf = pktbuf_alloc(sizeof(tcp_hdr_t));
    if (!buf)
    {
        debug_error(DEBUG_TCP, "no buffer");
        return NET_ERR_OK;
    }

    tcp_hdr_t * hdr = (tcp_hdr_t *) pktbuf_data(buf);
    plat_memset(hdr, 0, sizeof(tcp_hdr_t));
    hdr->sport = tcp->base.local_port;
    hdr->dport = tcp->base.remote_port;
    hdr->seq = tcp->snd.una;
    hdr->ack = tcp->rcv.nxt;
    hdr->flag = 0;
    hdr->f_syn = tcp->flags.syn_out;
    if (hdr->f_syn)
    {
        //write mss option
        write_sync_option(tcp, buf);
    }
    hdr->f_ack = tcp->flags.irs_valid;
    hdr->f_fin = (tcp_buf_cnt(&tcp->snd.buf) == 0) ? tcp->flags.fin_out : 0;
    hdr->win = tcp_rcv_window(tcp);
    hdr->urg_ptr = 0;

    tcp_set_hdr_size(hdr, buf->total_size);

    copy_send_data(tcp, buf, doff, dlen);
    tcp->snd.nxt = hdr->f_syn + hdr->f_fin + tcp->snd.una + dlen;
    return send_out(hdr, buf, &tcp->base.remote_ip, &tcp->base.local_ip);
}

static void tcp_out_timer_tmo(struct net_timer_t * timer, void * arg)
{
    tcp_t * tcp = (tcp_t *)arg;
    switch (tcp->snd.ostate) {
        case TCP_OSTATE_SENDING:
        {
            debug_warn(DEBUG_TCP, "tcp rexmit");
            net_err_t err = tcp_retransmit(tcp);
            if (err < 0)
            {
                debug_error(DEBUG_TCP, "rexmit failed");
                return;
            }
            tcp->snd.rexmit_cnt = 1;
            tcp->snd.rto *= 2;
            tcp->snd.ostate = TCP_OSTATE_REXMIT;
            net_timer_add(&tcp->snd.timer, tcp_ostate_name(tcp), tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        }
        case TCP_OSTATE_REXMIT:
        {
            if (++tcp->snd.rexmit_cnt > tcp->snd.rexmit_max)
            {
                debug_error(DEBUG_TCP, "rexmit failed");
                tcp_abort(tcp, NET_ERR_TMO);
                return;
            }
            debug_warn(DEBUG_TCP, "tcp rexmit");
            net_err_t err = tcp_retransmit(tcp);
            if (err < 0)
            {
                debug_error(DEBUG_TCP, "rexmit failed");
                return;
            }
            tcp->snd.rto *= 2;
            if(tcp->snd.rto >= TCP_RTO_MAX)
            {
                tcp->snd.rto = TCP_RTO_MAX;
            }
            net_timer_add(&tcp->snd.timer, tcp_ostate_name(tcp), tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        }
        default:
            break;
    }
}

void tcp_set_ostate(tcp_t * tcp, tcp_ostate_t state)
{
    if (state >= TCP_OSTATE_MAX)
    {
        return;
    }

    switch (state) {
        case TCP_OSTATE_IDLE:
            plat_printf("TCP_OSTATE_IDLE===================================\n");
            tcp->snd.rto = TCP_INIT_RTO;
            tcp->snd.ostate = TCP_OSTATE_IDLE;
            net_timer_remove(&tcp->snd.timer);
            break;
        case TCP_OSTATE_SENDING:
            tcp->snd.ostate = TCP_OSTATE_SENDING;
            net_timer_remove(&tcp->snd.timer);
            net_timer_add(&tcp->snd.timer, tcp_ostate_name(tcp), tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        case TCP_OSTATE_REXMIT:
            tcp->snd.ostate = TCP_OSTATE_REXMIT;
            net_timer_remove(&tcp->snd.timer);
            net_timer_add(&tcp->snd.timer, tcp_ostate_name(tcp), tcp_out_timer_tmo, tcp, tcp->snd.rto, 0);
            break;
        default:
            break;
    }
}

static void tcp_ostate_idle_in(tcp_t * tcp, tcp_oevent_t event)
{
    switch (event) {
        case TCP_OEVENT_SEND:
            tcp_transmit(tcp);
            tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
            break;

        default:
            break;
    }
}

static void tcp_ostate_sending_in(tcp_t * tcp, tcp_oevent_t event)
{
    switch (event) {
        case TCP_OEVENT_SEND:
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out)
            {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_in)
                {
                    debug_info(DEBUG_TCP, "tcp_buf_cnt-----------1");
                    tcp_transmit(tcp);
                    tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                }
                else
                {
                    debug_info(DEBUG_TCP, "tcp_buf_cnt-----------4");
                    tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
                }
            }
            break;

        default:
            break;
    }
}

static void tcp_ostate_resending_in(tcp_t * tcp, tcp_oevent_t event)
{
    switch (event) {
        case TCP_OEVENT_SEND:
            if ((tcp->snd.una == tcp->snd.nxt) || tcp->flags.fin_out)
            {
                if (tcp_buf_cnt(&tcp->snd.buf) || tcp->flags.fin_in)
                {
                    debug_info(DEBUG_TCP, "tcp_buf_cnt-----------2");
                    tcp_transmit(tcp);
                    tcp_set_ostate(tcp, TCP_OSTATE_SENDING);
                }
                else
                {
                    debug_info(DEBUG_TCP, "tcp_buf_cnt-----------4");
                    tcp_set_ostate(tcp, TCP_OSTATE_IDLE);
                }
            }
            else
            {
                debug_info(DEBUG_TCP, "tcp_buf_cnt-----------3");
                tcp_set_ostate(tcp, TCP_OSTATE_REXMIT);
                tcp_retransmit(tcp);
            }
            break;

        default:
            break;
    }

}

typedef void (*state_func_t) (tcp_t * t, tcp_oevent_t event);

void tcp_out_event(tcp_t * tcp, tcp_oevent_t event)
{
    static state_func_t state_func[] = {
            [TCP_OSTATE_IDLE] = tcp_ostate_idle_in,
            [TCP_OSTATE_SENDING] = tcp_ostate_sending_in,
            [TCP_OSTATE_REXMIT] = tcp_ostate_resending_in
    };

    if (tcp->snd.ostate >= TCP_OSTATE_MAX)
    {
        debug_error(DEBUG_TCP, "tcp state error: %s\n", tcp_ostate_name(tcp));
        return;
    }

    state_func[tcp->snd.ostate](tcp, event);
}