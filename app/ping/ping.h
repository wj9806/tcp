//
// Created by wj on 2024/5/6.
//

#ifndef NET_PING_H
#define NET_PING_H

#include <stdint.h>

#define PING_BUFFER_SIZE    4096
#define IP_ADDR_SIZE        4

#pragma pack(1)
typedef struct {
    //IHL
    uint16_t shdr : 4;
    //Version
    uint16_t version : 4;
    //type of service
    uint16_t tos : 8;
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_all;
    uint8_t ttl;
    uint8_t protocol;
    //header checksum
    uint16_t header_checksum;
    uint8_t src_ip[IP_ADDR_SIZE];
    uint8_t dest_ip[IP_ADDR_SIZE];
} ip_hdr_t;

typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_hdr_t;
#pragma pack()

typedef struct {
    icmp_hdr_t echo_hdr;
    char buf[PING_BUFFER_SIZE];
} echo_req_t;

typedef struct {
    ip_hdr_t ip_hdr;
    icmp_hdr_t echo_hdr;
    char buf[PING_BUFFER_SIZE];
} echo_reply_t;

typedef struct {
    echo_req_t req;
    echo_reply_t reply;
} ping_t;

/**
 * execute ping command
 * @param count execute count
 * @param size ping data packet size
 */
void ping_run(ping_t * ping, const char * dest, int count, int size, int interval);

#endif //NET_PING_H
