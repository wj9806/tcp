//
// Created by wj on 2024/5/23.
//

#include "tcp_state.h"
#include "tcp_out.h"
#include "tcp_in.h"

const char * tcp_state_name(tcp_state_t state)
{
    static const char * state_name[] = {
            [TCP_STATE_CLOSED] = "CLOSED",
            [TCP_STATE_LISTEN] = "LISTEN",
            [TCP_STATE_SYN_SENT] = "SYN_SENT",
            [TCP_STATE_SYN_RECV] = "SYN_RECV",
            [TCP_STATE_ESTABLISHED] = "ESTABLISHED",
            [TCP_STATE_FIN_WAIT_1] = "FIN_WAIT_1",
            [TCP_STATE_FIN_WAIT_2] = "FIN_WAIT_2",
            [TCP_STATE_CLOSING] = "CLOSING",
            [TCP_STATE_TIME_WAIT] = "TIME_WAIT",
            [TCP_STATE_CLOSE_WAIT] = "CLOSE_WAIT",
            [TCP_STATE_LAST_ACK] = "LAST_ACK",
            [TCP_STATE_MAX] = "UNKNOWN"
    };
    if (state > TCP_STATE_MAX)
    {
        state = TCP_STATE_MAX;
    }
    return state_name[state];
}

void tcp_set_state(tcp_t * tcp, tcp_state_t state)
{
    tcp->state = state;
    tcp_show_info("tcp set state", tcp);
}

net_err_t tcp_closed_in(tcp_t * tcp, tcp_seg_t * seg)
{
    if(seg->hdr->f_rst)
    {
        tcp_send_reset(seg);
    }
    return NET_ERR_OK;
}

net_err_t tcp_listen_in(tcp_t * tcp, tcp_seg_t * seg)
{
    return NET_ERR_OK;
}

net_err_t tcp_syn_sent_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;
    if (tcp_hdr->f_ack)
    {
        if ((tcp_hdr->ack - tcp->snd.iss <= 0) || (tcp_hdr->ack - tcp->snd.nxt > 0))
        {
            debug_warn(DEBUG_TCP, "%s: ack error", tcp_state_name(tcp->state));
            return tcp_send_reset(seg);
        }
    }
    if (tcp_hdr->f_rst)
    {
        if (!tcp_hdr->f_ack)
        {
            return NET_ERR_OK;
        }

        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        tcp->rcv.iss = tcp_hdr->seq;
        tcp->rcv.nxt = tcp_hdr->seq + 1;
        tcp->flags.irs_valid = 1;
        tcp_read_options(tcp, tcp_hdr);
        if (tcp_hdr->f_ack)
        {
            tcp_ack_process(tcp, seg);
        }
        if (tcp_hdr->f_ack)
        {
            //Three handshakes
            tcp_send_ack(tcp, seg);
            tcp_set_state(tcp, TCP_STATE_ESTABLISHED);
            sock_wakeup(&tcp->base, SOCK_WAIT_CONN, NET_ERR_OK);
        }
        else
        {
            //Four handshakes
            tcp_set_state(tcp, TCP_STATE_SYN_RECV);
            tcp_send_syn(tcp);
        }

    }
    return NET_ERR_OK;
}

net_err_t tcp_syn_recv_in(tcp_t * tcp, tcp_seg_t * seg)
{
    return NET_ERR_OK;
}

net_err_t tcp_established_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if(tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "ack process failed");
        return NET_ERR_UNREACHABLE;
    }
    tcp_data_in(tcp, seg);
    tcp_transmit(tcp);

    if (tcp->flags.fin_in)
    {
        tcp_set_state(tcp, TCP_STATE_CLOSE_WAIT);
    }
    return NET_ERR_OK;
}

static void tcp_time_wait(tcp_t * tcp)
{
    tcp_set_state(tcp, TCP_STATE_TIME_WAIT);
}

net_err_t tcp_fin_wait_1_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }
    tcp_data_in(tcp, seg);
    tcp_transmit(tcp);

    if (tcp->flags.fin_out == 0) {

        if (tcp->flags.fin_in)
        {
            tcp_time_wait(tcp);
        }
        else
        {
            tcp_set_state(tcp, TCP_STATE_FIN_WAIT_2);
        }
    }
    else if (tcp->flags.fin_in)
    {
        tcp_set_state(tcp, TCP_STATE_CLOSING);
    }
    return NET_ERR_OK;
}

net_err_t tcp_fin_wait_2_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }
    tcp_data_in(tcp, seg);

    if (tcp->flags.fin_in)
    {
        tcp_time_wait(tcp);
    }
    return NET_ERR_OK;
}

net_err_t tcp_closing_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }
    tcp_data_in(tcp, seg);
    tcp_transmit(tcp);
    if (tcp->flags.fin_out == 0)
    {
        tcp_time_wait(tcp);
    }
    return NET_ERR_OK;
}

net_err_t tcp_time_wait_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }

    if (tcp->flags.fin_in)
    {
        //send ack
        tcp_send_ack(tcp, seg);

        tcp_time_wait(tcp);
    }
    return NET_ERR_OK;
}

net_err_t tcp_close_wait_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }

    tcp_transmit(tcp);
    return NET_ERR_OK;
}

net_err_t tcp_last_ack_in(tcp_t * tcp, tcp_seg_t * seg)
{
    tcp_hdr_t * tcp_hdr = seg->hdr;

    if (tcp_hdr->f_rst)
    {
        debug_warn(DEBUG_TCP, "recv a rst");
        return tcp_abort(tcp, NET_ERR_RESET);
    }
    if (tcp_hdr->f_syn)
    {
        debug_warn(DEBUG_TCP, "recv a sync");
        tcp_send_reset(seg);
        return tcp_abort(tcp, NET_ERR_RESET);
    }

    if (tcp_ack_process(tcp, seg) < 0)
    {
        debug_warn(DEBUG_TCP, "tcp ack process failed");
        return NET_ERR_UNREACHABLE;
    }
    tcp_transmit(tcp);
    if (tcp->flags.fin_out == 0)
    {
        return tcp_abort(tcp, NET_ERR_CLOSE);
    }
    return NET_ERR_OK;
}




