#include "tcp_echo_server.h"
#include "sys_plat.h"
#include "net_api.h"

/**
 *  TCP server
 *
 *  socket ==> bind ==> listen ==> recv ==> send ==> close
 *                          ||
 *                          \/
 *                         accept
 */
void tcp_echo_server_start(int port)
{
    plat_printf("tcp echo server, port: %d\n", port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        plat_printf("tcp echo server: open socket failed\n");
        goto end;
    }

    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(netdev0_ip);
    server_addr.sin_port = htons(port);

    if (bind(s, (const struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        plat_printf("bind error\n");
        goto end;
    }
    listen(s, 5);
    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client = accept(s, (struct sockaddr *)&client_addr, &addr_len);
        if (client < 0)
        {
            plat_printf("accept error");
            break;
        }

        plat_printf("tcp echo server: connect ip: %s, port: %d\n",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        char buf[125];
        ssize_t size;
        while ((size = recv(client, buf, sizeof(buf), 0)) > 0)
        {
            plat_printf("recv size : %ld\n", size);
            send(client, buf, size, 0);
        }
        recv(client, buf, sizeof(buf), 0);
        close(client);
    }
end:
    if (s >= 0)
    {
        close(s);
    }
}
