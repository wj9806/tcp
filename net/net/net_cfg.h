//
// Created by xsy on 2024/3/15.
//

#ifndef NET_NET_CFG_H
#define NET_NET_CFG_H

#include "debug.h"

#define DEBUG_MBLOCK            DEBUG_LEVEL_INFO
#define DEBUG_QUEUE             DEBUG_LEVEL_INFO
#define DEBUG_MSG               DEBUG_LEVEL_INFO
#define DEBUG_PKTBUF            DEBUG_LEVEL_INFO
#define DEBUG_INIT              DEBUG_LEVEL_INFO
#define DEBUG_PLAT              DEBUG_LEVEL_INFO


#define EXMSG_MSG_CNT           10
#define PKTBUF_BLK_SIZE         128
#define PKTBUF_BLK_CNT          100
#define PKTBUF_BUF_CNT          100

#define EXMSG_LOCKER            LOCKER_THREAD

#define NETIF_HWADDR_SIZE       10
#define NETIF_NAME_SIZE         10
#define NETIF_INQ_SIZE            50
#define NETIF_OUTQ_SIZE           50

#endif //NET_NET_CFG_H
