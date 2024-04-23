//
// Created by wj on 2024/3/29.
//
#include "netif.h"
#include "mblock.h"
#include "pktbuf.h"
#include "exmsg.h"
#include "protocol.h"
#include "ether.h"

static netif_t netif_buffer[NETIF_DEV_CNT];
static mblock_t netif_block;
static list_t netif_list;
static netif_t * netif_default;

static const link_layer_t * link_layers[NETIF_TYPE_SIZE];

#if DEBUG_DISP_ENABLED(DEBUG_NETIF)
void display_netif_list(void)
{
    plat_printf("netif list: \n");
    node_t * node;
    list_for_each(node, &netif_list)
    {
        netif_t * netif = list_node_parent(node, netif_t, node);

        plat_printf("%s:", netif->name);
        switch (netif->state) {
            case NETIF_CLOSED:
                plat_printf(" %s ", "closed");
                break;
            case NETIF_OPENED:
                plat_printf(" %s ", "opened");
                break;
            case NETIF_ACTIVE:
                plat_printf(" %s ", "active");
                break;
            default:
                break;
        }

        switch (netif->type) {
            case NETIF_TYPE_ETHER:
                plat_printf(" %s ", "ether");
                break;
            case NETIF_TYPE_LOOP:
                plat_printf(" %s ", "loop");
                break;
            default:
                break;
        }

        plat_printf(" mtu=%d\n", netif->mtu);
        debug_dump_hwaddr("hwaddr: ", netif->hwaddr.addr, netif->hwaddr.len);
        debug_dump_ip(" ip:", &netif->ipaddr);
        debug_dump_ip(" netmask:", &netif->netmask);
        debug_dump_ip(" gateway:", &netif->gateway);
        plat_printf("\n");
    }
}
#else
#define display_netif_list()
#endif

net_err_t netif_init()
{
    debug_info(DEBUG_NETIF, "init netif");
    list_init(&netif_list);
    mblock_init(&netif_block, netif_buffer, sizeof(netif_t ), NETIF_DEV_CNT, LOCKER_NONE);

    netif_default = (netif_t *) 0;
    plat_memset(link_layers, 0, sizeof(link_layers));
    return NET_ERR_OK;
}

net_err_t netif_register_layer(int type, const link_layer_t * layer)
{
    if (type < 0 || type >= NETIF_TYPE_SIZE)
    {
        debug_error(DEBUG_NETIF, "type error:%d", type);
        return NET_ERR_PARAM;
    }

    if (link_layers[type])
    {
        debug_error(DEBUG_NETIF, "link layer exist£» %d", type);
        return NET_ERR_EXIST;
    }

    link_layers[type] = layer;
    return NET_ERR_OK;
}

static const link_layer_t * netif_get_layer(int type)
{
    if (type < 0 || type >= NETIF_TYPE_SIZE)
    {
        debug_error(DEBUG_NETIF, "type error:%d", type);
        return (link_layer_t *)0;
    }
    return link_layers[type];
}

netif_t * netif_open(const char * dev_name, const netif_ops_t * ops, void * ops_data)
{
    netif_t * netif = (netif_t *) mblock_alloc(&netif_block, - 1);
    if (!netif)
    {
        debug_error(DEBUG_NETIF, "no netif");
        return (netif_t *) 0;
    }
    ipaddr_set_any(&netif->ipaddr);
    ipaddr_set_any(&netif->netmask);
    ipaddr_set_any(&netif->gateway);

    plat_strncpy(netif->name, dev_name, NETIF_NAME_SIZE);
    netif->name[NETIF_NAME_SIZE - 1] = '\0';
    netif->type = NETIF_TYPE_NONE;
    netif->mtu = 0;
    node_init(&netif->node);
    net_err_t err = fixq_init(&netif->in_q, netif->in_q_buf, NETIF_INQ_SIZE, LOCKER_THREAD);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif in_q init failed");
        mblock_free(&netif_block, netif);
        return (netif_t *) 0;
    }
    err = fixq_init(&netif->out_q, netif->out_q_buf, NETIF_OUTQ_SIZE, LOCKER_THREAD);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif out_q init failed");
        fixq_destroy(&netif->in_q);
        mblock_free(&netif_block, netif);
        return (netif_t *) 0;
    }
    netif->ops = ops;
    netif->ops_data = ops_data;
    err = ops->open(netif, ops_data);
    if (err < 0)
    {
        debug_error(DEBUG_NETIF, "netif ops open err");
        goto error_handle;
    }
    netif->state = NETIF_OPENED;

    if (netif->type == NETIF_TYPE_NONE)
    {
        debug_error(DEBUG_NETIF, "netif type unknown");
        goto error_handle;
    }

    netif->link_layer = netif_get_layer(netif->type);
    if (!netif->link_layer && (netif->type != NETIF_TYPE_LOOP))
    {
        debug_error(DEBUG_NETIF, "link layer is null, netif name: %s", dev_name);
        goto error_handle;
    }

    list_insert_last(&netif_list, &netif->node);
    display_netif_list();
    return netif;
    error_handle:
    if (netif->state == NETIF_OPENED)
    {
        netif->ops->close(netif);
    }
    fixq_destroy(&netif->in_q);
    fixq_destroy(&netif->out_q);
    mblock_free(&netif_block, netif);
    return (netif_t *)0;
}

net_err_t netif_set_addr(netif_t * netif, ipaddr_t * ip, ipaddr_t * mask, ipaddr_t * gateway)
{
    ipaddr_copy(&netif->ipaddr, ip ? ip : ipaddr_get_any());
    ipaddr_copy(&netif->netmask, mask ? mask : ipaddr_get_any());
    ipaddr_copy(&netif->gateway, gateway ? gateway : ipaddr_get_any());
    return NET_ERR_OK;
}

net_err_t netif_set_hwaddr(netif_t  * netif, const char * hwaddr, int len)
{
    plat_memcpy(netif->hwaddr.addr, hwaddr, len);
    netif->hwaddr.len = len;
    return NET_ERR_OK;
}

void ipaddr_copy(ipaddr_t * dest, const ipaddr_t * src)
{
    if (!dest || !src)
    {
        return;
    }

    dest->type = src->type;
    dest->q_addr = src->q_addr;

}

ipaddr_t * ipaddr_get_any(void)
{
    static const ipaddr_t ipaddr_any = {.type = IPADDR_V4, .q_addr = 0};
    return (ipaddr_t *)&ipaddr_any;
}

net_err_t netif_set_active(netif_t * netif)
{
    if (netif->state != NETIF_OPENED)
    {
        debug_error(DEBUG_NETIF, "netif is not opened");
        return NET_ERR_STATE;
    }
    if (!netif_default && (netif->type != NETIF_TYPE_LOOP))
    {
        netif_set_default(netif);
    }

    if (netif->link_layer)
    {
        net_err_t err = netif->link_layer->open(netif);
        if (err < 0)
        {
            debug_error(DEBUG_NETIF, "link layer open failed");
            return err;
        }
    }

    netif->state = NETIF_ACTIVE;
    display_netif_list();
    return NET_ERR_OK;
}

net_err_t netif_set_deactive(netif_t * netif)
{
    if (netif->state != NETIF_ACTIVE)
    {
        debug_error(DEBUG_NETIF, "netif is not actived");
        return NET_ERR_STATE;
    }
    if (netif->link_layer)
    {
        netif->link_layer->close(netif);
    }

    //free pktbufs
    pktbuf_t * buf;
    while ((buf = fixq_recv(&netif->in_q, -1)) != (pktbuf_t *) 0)
    {
        pktbuf_free(buf);
    }
    while ((buf = fixq_recv(&netif->out_q, -1)) != (pktbuf_t *) 0)
    {
        pktbuf_free(buf);
    }
    if (netif_default == netif)
    {
        netif_default = (netif_t *) 0;
    }
    netif->state = NETIF_OPENED;
    display_netif_list();
    return NET_ERR_OK;
}

net_err_t netif_close(netif_t * netif)
{
    if (netif->state == NETIF_ACTIVE)
    {
        debug_error(DEBUG_NETIF, "netif is active");
        return NET_ERR_STATE;
    }

    netif->ops->close(netif);
    netif->state = NETIF_CLOSED;
    list_remove(&netif_list, &netif->node);
    mblock_free(&netif_block, netif);

    return NET_ERR_OK;
}

void netif_set_default(netif_t * netif)
{
    netif_default = netif;
}

netif_t * netif_get_default()
{
    return netif_default;
}

net_err_t netif_put_in(netif_t * netif, pktbuf_t * buf, int tmo)
{
    net_err_t err = fixq_send(&netif->in_q, buf, tmo);
    if (err < 0)
    {
        debug_warn(DEBUG_NETIF, "netif in_q is full");
        return NET_ERR_FULL;
    }

    exmsg_netif_in(netif);
    return NET_ERR_OK;
}

pktbuf_t * netif_get_in(netif_t * netif, int tmo)
{
    pktbuf_t * buf = fixq_recv(&netif->in_q, tmo);
    if (buf)
    {
        pktbuf_reset_access(buf);
        return buf;
    }

    debug_info(DEBUG_NETIF, "netif in_q empty");
    return (pktbuf_t *) 0;
}

net_err_t netif_put_out(netif_t * netif, pktbuf_t * buf, int tmo)
{
    net_err_t err = fixq_send(&netif->out_q, buf, tmo);
    if (err < 0)
    {
        debug_warn(DEBUG_NETIF, "netif out_q is full");
        return NET_ERR_FULL;
    }

    return NET_ERR_OK;
}

pktbuf_t * netif_get_out(netif_t * netif, int tmo)
{
    pktbuf_t * buf = fixq_recv(&netif->out_q, tmo);
    if (buf)
    {
        pktbuf_reset_access(buf);
        return buf;
    }

    debug_info(DEBUG_NETIF, "netif out_q empty");
    return (pktbuf_t *) 0;
}

net_err_t netif_out(netif_t * netif, ipaddr_t * ipaddr, pktbuf_t * buf)
{
    if (netif->link_layer)
    {
        //net_err_t err = ether_raw_out(netif, NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
        net_err_t err = netif->link_layer->out(netif, ipaddr, buf);
        if (err < 0)
        {
            debug_warn(DEBUG_NETIF, "netif link out err");
            return err;
        }
        return NET_ERR_OK;
    }
    else
    {
        net_err_t err = netif_put_out(netif, buf, -1);
        if (err < 0)
        {
            debug_error(DEBUG_NETIF, "send failed");
            return err;
        }
        return netif->ops->xmit(netif);
    }
}
