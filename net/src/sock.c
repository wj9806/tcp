//
// Created by wj on 2024/5/8.
//
#include "sock.h"
#include "sys_plat.h"

#define SOCKET_MAX_NR   10

static x_socket_t socket_tbl[SOCKET_MAX_NR];

/**
 * get the index of the socket in socket_tbl
 */
static int get_index(x_socket_t * s)
{
    return (int)(s - socket_tbl);
}

/**
 * get the socket at the specified index
 */
static x_socket_t * get_socket(int index)
{
    if (index < 0 || index >= SOCKET_MAX_NR)
        return (x_socket_t *)0;
    return socket_tbl + index;
}

/**
 * alloc a socket
 */
static x_socket_t * socket_alloc()
{
    x_socket_t * s = (x_socket_t*)0;
    for (int i = 0; i < SOCKET_MAX_NR; ++i) {
        x_socket_t * curr = socket_tbl + i;
        if (curr->state == SOCKET_STATE_FREE)
        {
            curr->state = SOCKET_STATE_USED;
            s = curr;
            break;
        }
    }
    return s;
}

/**
 * free socket
 */
static void socket_free(x_socket_t * s)
{
    s->state = SOCKET_STATE_FREE;
}


net_err_t socket_init()
{
    plat_memset(socket_tbl, 0, sizeof(socket_tbl));
    return NET_ERR_OK;
}