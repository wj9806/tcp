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

void tcp_buf_write_send(tcp_buf_t * buf, const uint8_t * buffer, int len)
{
    while (len > 0)
    {
        buf->data[buf->in++] = *buffer++;
        if (buf->in >= buf->size)
        {
            buf->in = 0;
        }
        len--;
        buf->count++;
    }
}

void tcp_buf_read_send(tcp_buf_t * buf, int offset, pktbuf_t * dest, int count)
{
    int free_for_us = buf->count - offset;
    if (count > free_for_us) {
        debug_warn(DEBUG_TCP, "resize for send: %d -> %d", count, free_for_us);
        count = free_for_us;
    }

    int start = buf->out + offset;
    if (start >= buf->size) {
        start -= buf->size;
    }

    while (count > 0) {
        // copy to array's tail
        int end = start + count;
        if (end >= buf->size) {
            end = buf->size;
        }
        int copy_size = (int)(end - start);

        // write data
        net_err_t err = pktbuf_write(dest, buf->data + start, (int)copy_size);
        assert(err >= 0, "write buffer failed.")

        // update start
        start += copy_size;
        if (start >= buf->size) {
            start -= buf->size;
        }
        count -= copy_size;

    }
}

int tcp_buf_remove(tcp_buf_t * buf, int cnt)
{
    if (cnt > buf->count)
    {
        cnt = buf->count;
    }

    buf->out += cnt;
    if(buf->out >= buf->size)
    {
        buf->out -= buf->size;
    }

    buf->count -= cnt;
    return cnt;
}

int tcp_buf_write_rcv(tcp_buf_t * dest, int offset, pktbuf_t * src, int total)
{
    int start = dest->in + offset;
    if (start >= dest->size) {
        start = start - dest->size;
    }

    int free_size = tcp_buf_free_cnt(dest) - offset;
    total = (total > free_size) ? free_size : total;

    int size = total;
    while (size > 0) {
        int free_to_end = dest->size - start;

        int curr_copy = size > free_to_end ? free_to_end : size;
        pktbuf_read(src, dest->data + start, (int)curr_copy);

        start += curr_copy;
        if (start >= dest->size) {
            start = start - dest->size;
        }

        dest->count += curr_copy;
        size -= curr_copy;
    }

    dest->in = start;
    return total;
}

int tcp_buf_read_rcv(tcp_buf_t * src, uint8_t * buf, int size)
{
    int total = size > src->count ? src->count : size;

    int curr_size = 0;
    while (curr_size < total) {
        *buf++ = src->data[src->out++];

        if (src->out >= src->size) {
            src->out = 0;
        }

        src->count--;
        curr_size++;
    }
    return total;
}