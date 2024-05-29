//
// Created by wj on 2024/5/29.
//

#include "tcp_buf.h"

void tcp_buf_init(tcp_buf_t * buf, uint8_t * data, int size)
{
    buf->data = data;
    buf->size = size;
    buf->in = buf->out = buf->count = 0;
}
