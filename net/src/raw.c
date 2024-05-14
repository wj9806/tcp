//
// Created by wj on 2024/5/8.
//

#include "raw.h"
#include "net_cfg.h"
#include "mblock.h"
#include "ipv4.h"
#include "socket.h"
#include "sock.h"

static raw_t raw_tbl[RAW_MAX_NR];
static mblock_t raw_mblock;
static list_t raw_list;

#if DEBUG_DISP_ENABLED(DEBUG_RAW)
static void display_raw_list(void)
{
    plat_printf("--------------raw list---------------\n");
    node_t * node;
    int idx = 0;

    list_for_each(node, &raw_list)
    {
        raw_t * raw = (raw_t *) list_node_parent(node, sock_t, node);
        plat_printf("[%d]:", idx++);
        debug_dump_ip("     local: ", &raw->base.local_ip);
        debug_dump_ip("     remote: ", &raw->base.remote_ip);
        plat_printf("\n");
    }
}
#else
#define display_raw_list()
#endif

net_err_t raw_init()
{
    debug_info(DEBUG_RAW, "raw init");
    list_init(&raw_list);
    mblock_init(&raw_mblock, raw_tbl, sizeof(raw_t), RAW_MAX_NR, LOCKER_NONE);
    return NET_ERR_OK;
}

static net_err_t raw_sendto(struct sock_t * s, const void * buf, size_t len, int flags,
                            const struct x_sockaddr * dest, x_socklen_t dest_len, ssize_t * result_len)
{
    ipaddr_t dest_ip;
    struct x_sockaddr_in * addr = (struct x_sockaddr_in *)dest;
    ipaddr_from_buf(&dest_ip, addr->sin_addr.addr_array);
    if (!ipaddr_is_any(&s->remote_ip) && !ipaddr_is_equal(&dest_ip, &s->remote_ip))
    {
        debug_error(DEBUG_RAW, "dest is incorrect");
        return NET_ERR_PARAM;
    }

    pktbuf_t * pkt_buf = pktbuf_alloc((int)len);
    if (!pkt_buf)
    {
        debug_error(DEBUG_RAW, "no buffer");
        return NET_ERR_NONE;
    }
    net_err_t err = pktbuf_write(pkt_buf, buf, (int)len);
    if (err < 0)
    {
        debug_error(DEBUG_RAW, "copy data error");
        goto end_send_to;
    }
    err = ipv4_out(s->protocol, &dest_ip, &s->local_ip, pkt_buf);
    if (err < 0)
    {
        debug_error(DEBUG_RAW, "send error");
        goto end_send_to;
    }
    *result_len = (ssize_t)len;
    return NET_ERR_OK;
    end_send_to:
    pktbuf_free(pkt_buf);
    return err;
}

static net_err_t raw_recvfrom(struct sock_t * s, void * buf, size_t len, int flags,
                    const struct x_sockaddr * src, x_socklen_t * src_len, ssize_t * result_len)
{
    raw_t * raw = (raw_t*)s;

    node_t * first = list_remove_first(&raw->recv_list);
    if (!first)
    {
        *result_len = 0;
        return NET_ERR_NEED_WAIT;
    }
    pktbuf_t * pktbuf = list_node_parent(first, pktbuf_t, node);
    ipv4_hdr_t * iphdr = (ipv4_hdr_t *) pktbuf_data(pktbuf);
    struct x_sockaddr_in * addr = (struct x_sockaddr_in*)src;
    plat_memset(addr, 0, sizeof(struct x_sockaddr_in));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    plat_memcpy(&addr->sin_addr, iphdr->src_ip, IPV4_ADDR_SIZE);

    int size = (pktbuf->total_size > (int)len) ? (int)len : pktbuf->total_size;

    pktbuf_reset_access(pktbuf);
    net_err_t err = pktbuf_read(pktbuf, buf, size);
    if (err < 0)
    {
        pktbuf_free(pktbuf);
        debug_error(DEBUG_RAW, "pktbuf read failed");
        return err;
    }

    pktbuf_free(pktbuf);
    *result_len = size;
    return NET_ERR_OK;
}

static net_err_t raw_close(sock_t * sock)
{
    raw_t * raw = (raw_t *) sock;
    list_remove(&raw_list, &sock->node);
    node_t * node;
    while ((node= list_remove_first(&raw->recv_list)))
    {
        pktbuf_t *buf = list_node_parent(node, pktbuf_t, node);
        pktbuf_free(buf);
    }

    sock_uninit(sock);
    mblock_free(&raw_mblock, sock);
    display_raw_list();
    return NET_ERR_OK;
}

static net_err_t raw_connect(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len)
{
    net_err_t err = sock_connect(s, addr, len);
    display_raw_list();
    return err;
}

static net_err_t raw_bind(struct sock_t * s, const struct x_sockaddr * addr, x_socklen_t len)
{
    net_err_t err = sock_bind(s, addr, len);
    display_raw_list();
    return err;
}

sock_t * raw_create(int family, int protocol)
{
    static const sock_ops_t raw_ops = {
            .sendto = raw_sendto,
            .recvfrom = raw_recvfrom,
            .setopt = sock_setopt,
            .close = raw_close,
            .send = sock_send,
            .recv = sock_recv,
            .connect = raw_connect,
            .bind = raw_bind,
    };
    raw_t * raw = mblock_alloc(&raw_mblock, -1);
    if (!raw)
    {
        debug_error(DEBUG_RAW, "no raw block");
        return (sock_t *)0;
    }

    net_err_t err = sock_init((sock_t*)raw, family, protocol, &raw_ops);
    if (err < 0)
    {
        debug_error(DEBUG_RAW, "init sock error");
        mblock_free(&raw_mblock, raw);
        return (sock_t *)0;
    }
    list_init(&raw->recv_list);

    raw->base.rcv_wait = &raw->rcv_wait;
    if (sock_wait_init(raw->base.rcv_wait))
    {
        debug_error(DEBUG_RAW, "create rcv wait failed");
        goto create_failed;
    }
    list_insert_last(&raw_list, &raw->base.node);
    return (sock_t*)raw;

    create_failed:
    sock_uninit(&raw->base);
    return (sock_t*)0;
}

//find suitable raw_t
static raw_t * raw_find(ipaddr_t * src, ipaddr_t * dest, uint8_t protocol)
{
    node_t * node;

    list_for_each(node, &raw_list)
    {
        raw_t * raw = (raw_t *)list_node_parent(node, sock_t, node);
        if (raw->base.protocol && (raw->base.protocol != protocol))
        {
            continue;
        }

        if (!ipaddr_is_any(&raw->base.remote_ip) && !ipaddr_is_equal(&raw->base.remote_ip, src))
        {
            continue;
        }

        if (!ipaddr_is_any(&raw->base.local_ip) && !ipaddr_is_equal(&raw->base.local_ip, dest))
        {
            continue;
        }

        return raw;
    }
    return (raw_t *)0;
}

net_err_t raw_in(pktbuf_t * pktbuf)
{
    ipv4_hdr_t * iphdr = (ipv4_hdr_t *) pktbuf_data(pktbuf);
    ipaddr_t src, dest;

    ipaddr_from_buf(&dest, iphdr->dest_ip);
    ipaddr_from_buf(&src, iphdr->src_ip);

    raw_t * raw = raw_find(&src, &dest, iphdr->protocol);
    if (!raw)
    {
        debug_warn(DEBUG_RAW, "no raw for req");
        return NET_ERR_UNREACHABLE;
    }

    if (list_count(&raw->recv_list) < RAW_MAX_RECV)
    {
        list_insert_last(&raw->recv_list, &pktbuf->node);
        sock_wakeup((sock_t *)raw, SOCK_WAIT_READ, NET_ERR_OK);
    }
    else
    {
        pktbuf_free(pktbuf);
    }
    return NET_ERR_OK;
}
