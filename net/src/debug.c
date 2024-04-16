//
// Created by xsy on 2024/3/14.
//

#include "debug.h"
#include "sys_plat.h"
#include <stdarg.h>

void debug_print(int module_level, int level, const char * file, const char * func, int line, const char * fmt, ...)
{
#ifdef EN_DEBUG
    if (module_level == DEBUG_LEVEL_NONE)
        return;
    //if the module_level greater than or equal the level, then print the log-info
    if (level <= module_level)
    {
        const char * end = file + plat_strlen(file);
        while (end >= file)
        {
            if ((*end == '\\') || (*end == '/'))
            {
                break;
            }

            end--;
        }
        end++;

        char * s_level;
        switch (level) {
            case DEBUG_LEVEL_ERROR:
                s_level = DEBUG_STYLE_RED "error";
                break;
            case DEBUG_LEVEL_WARN:
                s_level = DEBUG_STYLE_YELLOW "warn";
                break;
            default:
                s_level = DEBUG_STYLE_WHITE "info";
        }

        plat_printf("[%s] %s - %s:%d: " , s_level, end, func, line);

        char str_buf[128];
        va_list args;
                va_start(args, fmt);
        plat_vsprintf(str_buf, fmt, args);
        plat_printf("%s\n" DEBUG_STYLE_WHITE, str_buf);

        //recycling va_list
        va_end(args);
    }
#endif
}

void debug_dump_hwaddr(const char * msg, const uint8_t * hwaddr, int len)
{
    if (msg)
    {
        plat_printf("%s", msg);
    }

    if (len)
    {
        for (int i = 0; i < len; ++i) {
            if (i == len - 1)
            {
                plat_printf("%02x", hwaddr[i]);
            }
            else
            {
                plat_printf("%02x-", hwaddr[i]);
            }

        }
    }
    else
    {
        plat_printf("none");
    }

}

void debug_dump_ip(const char * msg, ipaddr_t * ipaddr)
{
    if (msg)
    {
        plat_printf("%s", msg);
    }

    if (ipaddr)
    {
        plat_printf("%d.%d.%d.%d", ipaddr->addr[0], ipaddr->addr[1], ipaddr->addr[2], ipaddr->addr[3]);
    }
    else
    {
        plat_printf("0.0.0.0");
    }
}

void debug_dump_ip_buf(const char * msg, uint8_t * ipaddr)
{
    if (msg)
    {
        plat_printf("%s", msg);
    }

    if (ipaddr)
    {
        plat_printf("%d.%d.%d.%d", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
    }
    else
    {
        plat_printf("0.0.0.0");
    }
}
