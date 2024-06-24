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
#include "timer.h"

static dns_entry_t dns_entry_tbl[DNS_ENTRY_SIZE];
static net_timer_t entry_update_timer;

static list_t req_list;
static mblock_t req_block;
static dns_req_t dns_req_tbl[DNS_REQ_SIZE];
static udp_t * dns_udp;
static uint16_t id;

static int cache_enable = 1;

static net_err_t dns_send_query(dns_req_t *req);

static void dns_req_fail(dns_req_t* req, net_err_t err);

static char working_buf[DNS_WORKING_BUF_SIZE];

#if DEBUG_DISP_ENABLED(DEBUG_DNS)
static void show_req_list (void) {
    plat_printf("dns req list----------\n");
    node_t * node;
    list_for_each(node, &req_list) {
        dns_req_t * req = list_node_parent(node, dns_req_t, node);

        plat_printf("name(%s), id: %d\n", req->domain_name, req->query_id);
    }
}

static void show_entry_list(void)
{
    plat_printf("cache list----------\n");
    for (int i = 0; i < DNS_ENTRY_SIZE; ++i) {
        dns_entry_t * entry = dns_entry_tbl + i;

        if (ipaddr_is_any(&entry->ipaddr))
        {
            continue;
        }

        plat_printf("%s ttl(%d)  ", entry->domain_name, entry->ttl);
        debug_dump_ip("ip:", &entry->ipaddr);
        plat_printf("\n");
    }
}
#else
#define show_req_list()
#define show_entry_list()
#endif

static void dns_entry_free(dns_entry_t * entry)
{
    ipaddr_set_any(&entry->ipaddr);
}

static void dns_update_tmo(struct net_timer_t * timer, void * arg)
{
    for (int i = 0; i < DNS_ENTRY_SIZE; ++i) {
        dns_entry_t * entry = dns_entry_tbl + i;
        if (ipaddr_is_any(&entry->ipaddr))
        {
            continue;
        }

        if(!entry->ttl || (--entry->ttl == 0))
        {
            dns_entry_free(entry);
            //show_entry_list();
        }
    }

    node_t * curr, * next;
    for (curr = list_first(&req_list); curr; curr = next) {
        next = list_node_next(curr);

        dns_req_t * req = list_node_parent(curr, dns_req_t, node);

        if (--req->retry_tmo == 0)
        {
            if (--req->retry_cnt == 0)
            {
                dns_req_fail(req, NET_ERR_TMO);
            }
            else
            {
                req->retry_tmo = DNS_QUERY_RETRY_TMO;
                dns_send_query(req);
            }
        }
    }
}

void dns_init(void)
{
    debug_info(DEBUG_DNS, "dns init");
    plat_memset(dns_entry_tbl, 0, sizeof(dns_entry_t));

    list_init(&req_list);
    mblock_init(&req_block, dns_req_tbl, sizeof(dns_req_t), DNS_REQ_SIZE, LOCKER_THREAD);

    net_timer_add(&entry_update_timer, "dns-refresher", dns_update_tmo, (void *)0, DNS_UPDATE_PERIOD * 1000, NET_TIMER_RELOAD);

    dns_udp = (udp_t *) udp_create(AF_INET, IPPROTO_UDP);
    assert(dns_udp != (udp_t *)0, "dns_udp create failed")
}

dns_req_t * dns_alloc_req(void)
{
    return mblock_alloc(&req_block, 0);
}

void dns_free_req(dns_req_t * req)
{
    if (req->wait_sem != SYS_SEM_INVALID)
    {
        sys_sem_free(req->wait_sem);
        req->wait_sem = SYS_SEM_INVALID;
    }
    mblock_free(&req_block, req);
}

static void dns_entry_init(dns_entry_t * entry, const char * domain_name, int ttl, ipaddr_t * ipaddr)
{
    ipaddr_copy(&entry->ipaddr, ipaddr);
    entry->ttl = ttl;
    plat_strncpy(entry->domain_name, domain_name, DNS_DOMAIN_MAX - 1);
    entry->domain_name[DNS_DOMAIN_MAX - 1] = '\0';
}

static void dns_entry_insert(const char * domain_name, uint32_t ttl, ipaddr_t * ipaddr)
{
    dns_entry_t * next = (dns_entry_t *) 0;
    dns_entry_t * oldest = (dns_entry_t *)0;

    for (int i = 0; i < DNS_ENTRY_SIZE; ++i) {
        dns_entry_t * entry = dns_entry_tbl + i;
        if (ipaddr_is_any(&entry->ipaddr))
        {
            next = entry;
            break;
        }

        if((oldest == (dns_entry_t *)0) || (entry->ttl < oldest->ttl))
        {
            oldest = entry;
        }
    }

    next = next ? next : oldest;
    dns_entry_init(next, domain_name, ttl, ipaddr);
    show_entry_list();
}

static dns_entry_t * dns_entry_find(const char * domain_name)
{
    for (int i = 0; i < DNS_ENTRY_SIZE; ++i) {
        dns_entry_t * curr = dns_entry_tbl + i;
        if(!ipaddr_is_any(&curr->ipaddr))
        {
            if(plat_stricmp(domain_name, curr->domain_name) == 0)
            {
                return curr;
            }
        }
    }
    return (dns_entry_t *) 0;
}

static void dns_req_add(dns_req_t * req)
{
    req->retry_tmo = DNS_QUERY_RETRY_TMO;
    req->retry_cnt = DNS_QUERY_RETRY_CNT;
    req->query_id = ++id;
    req->err = NET_ERR_OK;
    ipaddr_set_any(&req->ipaddr);
    list_insert_last(&req_list, &req->node);

    if (list_count(&req_list))
    {
        show_req_list();
    }
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
    plat_printf("%s\n", domain_name);
    return (uint8_t*) f + sizeof(dns_qfield_t);
}

static net_err_t dns_send_query(dns_req_t *req) {
    dns_hdr_t * dns_hdr = (dns_hdr_t *) working_buf;
    dns_hdr->id = x_htons(req->query_id);
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

    if (cache_enable)
    {
        dns_entry_t * dns_entry = dns_entry_find(dns_req->domain_name);
        if (dns_entry)
        {
            ipaddr_copy(&dns_req->ipaddr, &dns_entry->ipaddr);
            dns_req->err = NET_ERR_OK;
            return NET_ERR_OK;
        }
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

static const char * domain_name_cmp(const char * domain_name, const char * name, size_t size)
{
    const char * src = domain_name;
    const char * dest = name;

    while (*src)
    {
        int cnt = *dest++;
        for (int i = 0; i < cnt && *src && *dest; ++i) {
            if (*dest++ != *src++)
            {
                return (const char *)0;
            }
        }

        if(*src == '\0')
        {
            break;
        }
        else if (*src++ != '.')
        {
            return (const char *)0;
        }
    }

    return (dest >= name + size) ? (const char * )0 : dest + 1;
}

static const uint8_t * domain_name_skip(const uint8_t* name, size_t size)
{
    const uint8_t * c = name;
    const uint8_t * end = name + size;

    while (*c && (c < end))
    {
        if((*c & 0xc0) == 0xc0)
        {
            c += 2;
            goto skip;
        }
        else
        {
            c += *c;
        }
    }
    if(c < end)
    {
        c++;
    }
    else
    {
        return (const uint8_t*) 0;
    }
    skip:
    return c;
}

static void dns_req_remove(dns_req_t * req, net_err_t err)
{
    list_remove(&req_list, &req->node);

    req->err = err;
    if (req->err < 0)
    {
        ipaddr_set_any(&req->ipaddr);
    }

    sys_sem_notify(req->wait_sem);
    sys_sem_free(req->wait_sem);
    req->wait_sem = SYS_SEM_INVALID;

    show_req_list();
}

static void dns_req_fail(dns_req_t* req, net_err_t err)
{
    dns_req_remove(req, err);
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

    node_t * node;
    list_for_each(node, &req_list)
    {
        dns_req_t * req = list_node_parent(node, dns_req_t, node);

        if (req->query_id != dns_hdr->id)
        {
            continue;
        }

        if (dns_hdr->flags.qr == 0)
        {
            debug_warn(DEBUG_DNS, "not a resp");
            goto req_failed;
        }

        if (dns_hdr->flags.tc == 1)
        {
            debug_warn(DEBUG_DNS, "truct");
            goto req_failed;
        }

        if(dns_hdr->flags.ra == 0)
        {
            debug_warn(DEBUG_DNS, "recursion not support");
            goto req_failed;
        }

        switch (dns_hdr->flags.rcode) {
            case DNS_ERR_NONE:
                break;
            case DNS_ERR_NOTIMP:
                debug_warn(DEBUG_DNS, "server reply: not support");
                err = NET_ERR_NOT_SUPPORT;
                goto req_failed;
            default:
                debug_warn(DEBUG_DNS, "unknown err: %d", dns_hdr->flags.rcode);
                err = NET_ERR_UNKNOWN;
                goto req_failed;
        }

        if(dns_hdr->qdcount == 1)
        {
            rcv_start += sizeof(dns_hdr_t);
            rcv_start = (const uint8_t *)domain_name_cmp(req->domain_name, (const char *)rcv_start, rcv_end - rcv_start);
            if(rcv_start == (uint8_t*) 0)
            {
                debug_warn(DEBUG_DNS, "domain name not match");
                err = NET_ERR_BROKEN;
                goto req_failed;
            }

            if (rcv_start + sizeof(dns_qfield_t) > rcv_end)
            {
                debug_warn(DEBUG_DNS, "size error");
                err = NET_ERR_SIZE;
                goto req_failed;
            }

            dns_qfield_t * qf = (dns_qfield_t*)rcv_start;
            if (qf->class != x_ntohs(DNS_QUERY_CLASS_INET))
            {
                debug_warn(DEBUG_DNS, "query class not inet");
                err = NET_ERR_BROKEN;
                goto req_failed;
            }

            if (qf->type != x_ntohs(DNS_QUERY_TYPE_A))
            {
                debug_warn(DEBUG_DNS, "query type not A");
                err = NET_ERR_BROKEN;
                goto req_failed;
            }
            rcv_start += sizeof(dns_qfield_t);
        }

        if (dns_hdr->ancount < 1)
        {
            debug_warn(DEBUG_DNS, "query answer == 0");
            err = NET_ERR_NONE;
            goto req_failed;
        }
        for (int i = 0; (i < dns_hdr->ancount) && (rcv_start < rcv_end); ++i)
        {
            rcv_start = domain_name_skip(rcv_start, rcv_end - rcv_start);
            if (rcv_start == (uint8_t*) 0)
            {
                debug_warn(DEBUG_DNS, "size error");
                err = NET_ERR_BROKEN;
                goto req_failed;
            }

            if (rcv_start + sizeof(dns_afield_t) > rcv_end)
            {
                debug_warn(DEBUG_DNS, "size error");
                err = NET_ERR_BROKEN;
                goto req_failed;
            }

            dns_afield_t * af = (dns_afield_t *) rcv_start;
            if ((af->class == x_ntohs(DNS_QUERY_CLASS_INET))
                && (af->type == x_ntohs(DNS_QUERY_TYPE_A))
                && (af->rd_len == x_ntohs(IPV4_ADDR_SIZE)))
            {
                ipaddr_from_buf(&req->ipaddr, (const uint8_t*)af->rdata);
                dns_entry_insert(req->domain_name, x_ntohl(af->ttl), &req->ipaddr);
                dns_req_remove(req, NET_ERR_OK);
                return;
            }
            rcv_start += sizeof(dns_afield_t) + x_ntohs(af->rd_len) - 1;
        }
        req_failed:
        dns_req_fail(req, err);
        return;
    }
}

