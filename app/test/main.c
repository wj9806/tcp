#include <stdio.h>
#include "sys_plat.h"
#include "echo/tcp_echo_client.h"
#include "echo/tcp_echo_server.h"

#define PORT       5000

static int count = 0;

int main()
{
    //tcp_echo_client_start(friend0_ip, PORT);
    tcp_echo_server_start(PORT);

    pcap_t * pcap = pcap_device_open(netdev0_phy_ip, netdev0_hwaddr);
    while (pcap)
    {
        static uint8_t buffer[1024];
        static  int counter = 0;
        struct pcap_pkthdr * pkthdr;
        const uint8_t * pkt_data;

        plat_printf("begin test: %d\n", counter++);
        for (int i = 0; i < sizeof (buffer); ++i) {
            buffer[i] = i;
        }

        if (pcap_next_ex(pcap, &pkthdr, &pkt_data) != 1)
        {
            continue;
        }

        //received packet
        int len = pkthdr->len > sizeof(buffer) ? sizeof(buffer) : pkthdr->len;
        plat_memcpy(buffer, pkt_data, len);
        buffer[0] = 1;
        buffer[1] = 2;

        if (pcap_inject(pcap, buffer, len) == -1)
        {
            plat_printf("pcap send: send packet failed %s\n", pcap_geterr(pcap));
            break;
        }

    }
    return 0;
}