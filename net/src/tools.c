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
