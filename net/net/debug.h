//
// Created by xsy on 2024/3/14.
//

#ifndef NET_DEBUG_H
#define NET_DEBUG_H

#define EN_DEBUG

#define DEBUG_STYLE_RED         "\033[31m"
#define DEBUG_STYLE_GREEN       "\033[32m"
#define DEBUG_STYLE_YELLOW      "\033[33m"
#define DEBUG_STYLE_WHITE       "\033[0m"

#define DEBUG_LEVEL_NONE        0
#define DEBUG_LEVEL_ERROR       1
#define DEBUG_LEVEL_WARN        2
#define DEBUG_LEVEL_INFO        3

void debug_print(int module, int level, const char * file, const char * func, int line, const char * fmt, ...);

#define debug_info(module, fmt, ...)  debug_print(module, DEBUG_LEVEL_INFO, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define debug_warn(module, fmt, ...)  debug_print(module, DEBUG_LEVEL_WARN, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define debug_error(module, fmt, ...) debug_print(module, DEBUG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)


#define assert(expr, msg) { \
    if(!(expr))             \
    {                       \
        debug_print(DEBUG_LEVEL_ERROR, DEBUG_LEVEL_ERROR, __FILE__, __FUNCTION__, __LINE__, "assert failed: "#expr", "msg);       \
        while(1);           \
    }                       \
}

#endif //NET_DEBUG_H
