//
// Created by wj on 2024/4/8.
//
#include "tools.h"
#include "debug.h"

static int is_little_endian()
{
    uint16_t v = 0x1234;
    return *(uint8_t *)&v == 0x34;
}

net_err_t tools_init(void)
{
    debug_info(DEBUG_TOOLS, "init tools");
    if (is_little_endian() != NET_ENDIAN_LITTLE)
    {
        debug_error(DEBUG_TOOLS, "tools check error");
        return NET_ERR_SYS;
    }
    return NET_ERR_OK;
}

uint16_t checksum16(int offset, void * buf, uint16_t len, uint32_t pre_sum, int complement)
{
    uint16_t * curr_buf = (uint16_t *)buf;

    uint32_t checksum = pre_sum;
    if (offset & 0x1)
    {
        uint8_t * b = (uint8_t*)curr_buf;
        checksum += *b++ << 8;
        curr_buf = (uint16_t *) b;
        len--;
    }
    while (len > 1)
    {
        checksum += *curr_buf++;
        len -= 2;
    }

    if (len > 0)
    {
        checksum += *(uint8_t *) curr_buf;
    }

    uint16_t high;
    while ((high = checksum >> 16) != 0)
    {
        checksum = high + (checksum & 0xFFFF);
    }

    return complement ? (uint16_t) ~checksum : (uint16_t) checksum;
}

uint16_t checksum_peso(pktbuf_t * buf, const ipaddr_t * dest, const ipaddr_t * src, uint8_t protocol)
{
    uint8_t zero_protocol[2] = {0, protocol};
    int offset = 0;
    uint32_t sum = checksum16(offset, (void *)src->addr, IPV4_ADDR_SIZE,0, 0);
    offset += IPV4_ADDR_SIZE;

    sum = checksum16(offset, (void *)dest->addr, IPV4_ADDR_SIZE, sum, 0);
    offset += IPV4_ADDR_SIZE;

    sum = checksum16(offset, (void *)zero_protocol, 2, sum, 0);
    offset += 2;

    uint16_t len = x_htons(buf->total_size);
    sum = checksum16(offset, &len, 2, sum, 0);

    pktbuf_reset_access(buf);
    sum = pktbuf_checksum16(buf, buf->total_size, (int)sum, 1);
    return sum;
}
