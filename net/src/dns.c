//
// Created by wj on 2024/6/17.
//

#include "dns.h"
#include "mblock.h"
#include "list.h"
#include "debug.h"
#include "tools.h"
#include "net_api.h"
#include "socket.h"

static list_t req_list;
static mblock_t req_block;
static dns_req_t dns_req_tbl[DNS_REQ_SIZE];
static udp_t * dns_udp;

static char working_buf[DNS_WORKING_BUF_SIZE];

void dns_init(void)
{
    debug_info(DEBUG_DNS, "dns init");
    list_init(&req_list);
    mblock_init(&req_block, dns_req_tbl, sizeof(dns_req_t), DNS_REQ_SIZE, LOCKER_THREAD);

    dns_udp = (udp_t *) udp_create(AF_INET, IPPROTO_UDP);
    assert(dns_udp != (udp_t *)0, "dns_udp create failed")
}

dns_req_t * dns_alloc_req(void)
{
    static dns_req_t req;
    return &req;
}

void dns_free_req(dns_req_t * req)
{

}

static dns_entry_t * dns_entry_find(const char * domain_name)
{
    return (dns_entry_t *) 0;
}

static void dns_req_add(dns_req_t * req)
{

}

static uint8_t * ip_add_query_find(char * domain_name, uint8_t * buf, size_t size)
{
    if (size < plat_strlen(domain_name) + 2 + 4)
    {
        debug_error(DEBUG_DNS, "no buf");
        return (uint8_t *)0;
    }

    char * name_buf = buf;
    name_buf[0] = '.';
    plat_strcpy(name_buf + 1, domain_name);

    char * c = name_buf;
    while (*c)
    {
        if (*c == '.')
        {
            char * dot = c++;
            while (*c && (*c != '.'))
            {
                c++;
            }
            *dot = (uint8_t)(c - dot - 1);
        }
        else
        {
            c++;
        }
    }
    *c++ = '\0';

    dns_qfield_t * f = (dns_qfield_t*)c;
    f->class = x_htons(DNS_QUERY_CLASS_INET);
    f->type = x_htons(DNS_QUERY_TYPE_A);

    return (uint8_t*) f + sizeof(dns_qfield_t);
}

static net_err_t dns_send_query(dns_req_t *req) {
    dns_hdr_t * dns_hdr = (dns_hdr_t *) working_buf;
    dns_hdr->id = x_htons(0);
    dns_hdr->flags.all = 0;
    dns_hdr->flags.qr = 0;
    dns_hdr->flags.rd = 1;
    dns_hdr->flags.all = x_htons(dns_hdr->flags.all);
    dns_hdr->qdcount = x_htons(1);
    dns_hdr->ancount = 0;
    dns_hdr->nscount = 0;
    dns_hdr->arcount = 0;

    uint8_t * buf = working_buf + sizeof(dns_hdr_t);
    buf = ip_add_query_find(req->domain_name, buf, sizeof(working_buf) - ((char *)buf - working_buf));
    if(!buf) {
        debug_error(DEBUG_DNS, "add query failed");
        return NET_ERR_MEM;
    }

    struct x_sockaddr_in dest;
    plat_memset(&dest,0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = x_htons(DNS_PORT);
    dest.sin_addr.s_addr = x_inet_addr("192.168.0.1");
    return udp_sendto((sock_t *) dns_udp, working_buf, (char *)buf - working_buf, 0,
                      (const struct x_sockaddr *)&dest,sizeof(dest), (ssize_t*)0);
}

int dns_is_arrive(udp_t * udp)
{
    return udp == dns_udp;
}

net_err_t dns_req_in(func_msg_t * msg)
{
    dns_req_t * dns_req = (dns_req_t*) msg->param;
    if (plat_strcmp(dns_req->domain_name, "localhost") == 0)
    {
        ipaddr_from_str(&dns_req->ipaddr, "127.0.0.1");
        dns_req->err = NET_ERR_OK;
        return NET_ERR_OK;
    }

    ipaddr_t ipaddr;
    if (ipaddr_from_str(&ipaddr, dns_req->domain_name) == NET_ERR_OK)
    {
        ipaddr_copy(&dns_req->ipaddr, &ipaddr);
        dns_req->err = NET_ERR_OK;
        return NET_ERR_OK;
    }

    dns_entry_t * dns_entry = dns_entry_find(dns_req->domain_name);
    if (dns_entry)
    {
        ipaddr_copy(&dns_req->ipaddr, &dns_entry->ipaddr);
        dns_req->err = NET_ERR_OK;
        return NET_ERR_OK;
    }

    dns_req->wait_sem = sys_sem_create(0);
    if (dns_req->wait_sem == SYS_SEM_INVALID)
    {
        debug_error(DEBUG_DNS, "create wait sem failed");
        return NET_ERR_SYS;
    }

    dns_req_add(dns_req);
    net_err_t err = dns_send_query(dns_req);
    if (err < 0)
    {
        debug_error(DEBUG_DNS, "send dns req failed");
        goto dns_req_failed;
    }
    return NET_ERR_OK;

    dns_req_failed:
    if (dns_req->wait_sem != SYS_SEM_INVALID)
    {
        sys_sem_free(dns_req->wait_sem);
        dns_req->wait_sem = SYS_SEM_INVALID;
    }
    return err;
}

void dns_in()
{
    ssize_t rcv_len;
    struct x_sockaddr_in src;
    x_socklen_t addr_len;
    net_err_t err = udp_recvfrom((sock_t *)dns_udp, working_buf, sizeof(working_buf), 0,
                                 (struct x_sockaddr*) &src, &addr_len, &rcv_len);
    if (err < 0)
    {
        debug_error(DEBUG_DNS, "rcv udp failed");
        return;
    }

    const uint8_t * rcv_start = (uint8_t *) working_buf;
    const uint8_t * rcv_end = (uint8_t *) working_buf + rcv_len;

    dns_hdr_t * dns_hdr = (dns_hdr_t *)rcv_start;
    dns_hdr->id = x_ntohs(dns_hdr->id);
    dns_hdr->flags.all = x_ntohs(dns_hdr->flags.all);
    dns_hdr->qdcount = x_ntohs(dns_hdr->qdcount);
    dns_hdr->ancount = x_ntohs(dns_hdr->ancount);
    dns_hdr->nscount = x_ntohs(dns_hdr->nscount);
    dns_hdr->arcount = x_ntohs(dns_hdr->arcount);
}

