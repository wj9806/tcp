//
// Created by wj on 2024/3/20.
//

#ifndef NET_PKTBUF_H
#define NET_PKTBUF_H

#include "list.h"
#include <stdint.h>
#include "net_cfg.h"
#include "net_err.h"

//data block struct
typedef struct pktblk_t
{
    node_t node;
    int size;
    //data addr
    uint8_t * data;
    uint8_t payload[PKTBUF_BLK_SIZE];
} pktblk_t;


typedef struct _pktbuf_t
{
    int total_size;
    //data block list
    list_t blk_list;
    //data packet node
    node_t node;
} pktbuf_t;

net_err_t pktbuf_init(void);

#endif //NET_PKTBUF_H
