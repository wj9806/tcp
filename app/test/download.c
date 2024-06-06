//
// Created by wj on 2024/6/3.
//

#include <stdio.h>
#include <string.h>
#include "sys_plat.h"
#include "net_api.h"

void download_test(const char * filename, int port)
{
    printf("try to download %s from %s:%d\n", filename, friend0_ip, port);

    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0)
    {
        printf("create socket failed\n");
        goto failed;
    }

    FILE * file = fopen(filename, "wb");
    if(file == (FILE *)0)
    {
        printf("open file failed");
        goto failed;
    }

    struct sockaddr_in server_addr;
    plat_memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(friend0_ip);
    server_addr.sin_port = htons(port);

    if (connect(sockfd, (const struct sockaddr*) &server_addr, sizeof(server_addr)) < 0)
    {
        plat_printf("connect error\n");
        goto failed;
    }
    else
    {
        plat_printf("connect success\n");
    }

    static char buf[8192];
    ssize_t total_size = 0;
    int rcv_size = 0;
    while ((rcv_size = recv(sockfd, buf, sizeof(buf), 0)) > 0)
    {
        fwrite(buf, 1, rcv_size, file);
        //flush buffer to file
        fflush(file);
        printf(".");
        total_size += rcv_size;
    }
    if (rcv_size < 0)
    {
        printf("rcv size %d\n", rcv_size);
        goto failed;
    }
    printf("rcv file size : %d\n", (int) total_size);
    printf("rcv file ok\n");
    closesocket(sockfd);
    fclose(file);
    return;

    failed:
    closesocket(sockfd);
    if(file)
    {
        fclose(file);
    }
}
