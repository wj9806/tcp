//
// Created by wj on 2024/5/29.
//

#ifndef NET_TCP_BUF_H
#define NET_TCP_BUF_H

#include "stdint.h"
#include "pktbuf.h"

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

/**
 * write data to send buffer cache
 */
void tcp_buf_write_send(tcp_buf_t * buf, const uint8_t * buffer, int len);

/**
 * read tcp_buf_t into pktbuf_t
 */
void tcp_buf_read_send(tcp_buf_t * buf, int offset, pktbuf_t * dest, int count);

/**
 * write data to rcv buffer cache
 */
int tcp_buf_write_rcv(tcp_buf_t * dest, int offset, pktbuf_t * src, int total);

/**
 * read tcp_buf_t into buf
 */
int tcp_buf_read_rcv(tcp_buf_t * src, uint8_t * buf, int size);

/**
 * remove buf
 */
int tcp_buf_remove(tcp_buf_t * buf, int cnt);

#endif //NET_TCP_BUF_H
