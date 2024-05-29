//
// Created by wj on 2024/5/29.
//

#ifndef NET_TCP_BUF_H
#define NET_TCP_BUF_H
#include "stdint.h"

typedef struct {
    uint8_t * data;
    int count;
    int size;
    int in, out;
} tcp_buf_t;

//init tcp buf
void tcp_buf_init(tcp_buf_t * buf, uint8_t * data, int size);

//get tcp buf size
static inline int tcp_buf_size(tcp_buf_t * buf)
{
    return buf->size;
}

//get tcp buf count
static inline int tcp_buf_cnt(tcp_buf_t * buf)
{
    return buf->count;
}

//get tcp buf free count
static inline int tcp_buf_free_cnt(tcp_buf_t * buf)
{
    return buf->size - buf->count;
}

#endif //NET_TCP_BUF_H
