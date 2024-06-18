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

net_err_t dns_req_in(func_msg_t * msg)
{
    return NET_ERR_OK;
}

