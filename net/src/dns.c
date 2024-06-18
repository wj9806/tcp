//
// Created by wj on 2024/6/17.
//

#include "dns.h"
#include "mblock.h"
#include "list.h"
#include "debug.h"

static list_t req_list;
static mblock_t req_block;
static dns_req_t dns_req_tbl[DNS_REQ_SIZE];

void dns_init(void)
{
    debug_info(DEBUG_DNS, "dns init");
    list_init(&req_list);
    mblock_init(&req_block, dns_req_tbl, sizeof(dns_req_t), DNS_REQ_SIZE, LOCKER_THREAD);
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

static net_err_t dns_send_query(dns_req_t *req) {
    return NET_ERR_OK;
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

