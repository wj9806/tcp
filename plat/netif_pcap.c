//
// Created by xsy on 2024/3/14.
//
#include "netif_pcap.h"
#include "sys_plat.h"
#include "exmsg.h"

//data packet recv thread
void recv_thread (void * arg)
{
    plat_printf("recv thread is running!\n");
    netif_t * netif = (netif_t *) arg;
    pcap_t * pcap = (pcap_t *) netif->ops_data;
    while (1)
    {
        struct pcap_pkthdr * pkthdr;
        const uint8_t * pkt_data;
        if (pcap_next_ex(pcap, &pkthdr, &pkt_data) != 1)
        {
            continue;
        }
        pktbuf_t * buf = pktbuf_alloc(pkthdr->len);
        if (buf == (pktbuf_t *)0)
        {
            debug_warn(DEBUG_NETIF, "buf == NULL");
            continue;
        }

        pktbuf_write(buf, pkt_data, pkthdr->len);

        if (netif_put_in(netif, buf, 0) < 0)
        {
            debug_warn(DEBUG_NETIF, "netif %s in_q full\n", netif->name);
            pktbuf_free(buf);
            continue;
        }
    }
}

//data packet xmit thread
void xmit_thread (void * arg)
{
    plat_printf("xmit thread is running!\n");

    while (1)
    {
        sys_sleep(1);
    }
}

net_err_t netif_pcap_open(struct netif_t * netif, void * data)
{
    pcap_data_t * pcap_data = (pcap_data_t *) data;
    pcap_t * pcap = pcap_device_open(pcap_data->ip, pcap_data->hwaddr);
    if (pcap == (pcap_t *)0)
    {
        debug_error(DEBUG_NETIF, "pcap open failed, name: %s\n", netif->name);
        return NET_ERR_IO;
    }

    netif->type = NETIF_TYPE_ETHER;
    netif->mtu = 1500;
    netif->ops_data = pcap;
    netif_set_hwaddr(netif, (const char *) pcap_data->hwaddr, 6);

    sys_thread_create(recv_thread, netif);
    sys_thread_create(xmit_thread, netif);
    return NET_ERR_OK;
}

void netif_pcap_close(struct netif_t * netif)
{
    pcap_close(netif->ops_data);
}

net_err_t netif_pcap_xmit(struct netif_t * netif)
{
    return NET_ERR_OK;
}

const struct netif_ops_t netdev_ops = {
    .open = netif_pcap_open,
    .close = netif_pcap_close,
    .xmit = netif_pcap_xmit,
};
