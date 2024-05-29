#include "tcp_echo_client.h"
#include "sys_plat.h"
#include "net_api.h"

/**
 * TCP client
 *
 * socket ==> connect ==> send ==> recv ==> close
 *
 */
int tcp_echo_client_start (const char * ip, int port)
{
    plat_printf("tcp echo client, ip: %s, port: %d\n", ip, port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        plat_printf("tcp echo client: open socket failed\n");
        goto end;
    }
    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    if (connect(s, (const struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        plat_printf("connect error\n");
        goto end;
    }
    else
    {
        plat_printf("connect success\n");
    }

#if 0
    char sbuf[128];
    //fgets(sbuf, sizeof(sbuf), stdin);
    close(s);
    return 0;
#endif

#if 1
    char sbuf[TCP_SBUF_SIZE];
    for (int i = 0; i < sizeof(sbuf); ++i) {
        sbuf[i] = 'a' + i %26;
    }
    for (int i = 0; i < 10; ++i) {
        ssize_t size = send(s, sbuf, sizeof(sbuf), 0);
        if (size < 0)
        {
            printf("send error\n");
        }
    }
    fgets(sbuf, sizeof(sbuf), stdin);
    close(s);
    return 0;
#endif

    char buf[128];
    plat_printf(">>");
    while (fgets(buf, sizeof(buf), stdin) != NULL)
    {
        if (send(s, buf, plat_strlen(buf) - 1, 0) <= 0)
        {
            plat_printf("write error\n");
            goto end;
        }
        plat_memset(buf, 0, sizeof(buf));
        int len = recv(s, buf, sizeof(buf) - 1, 0);
        if (len <= 0)
        {
            plat_printf("read error\n");
            goto end;
        }
        plat_printf("%s", buf);
        plat_printf(">>");
    }

    close(s);
    return 1;
end:
    if (s >= 0)
    {
        close(s);
    }
    return -1;
}