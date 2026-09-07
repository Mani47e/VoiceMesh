#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NET_AUDIO_PAYLOAD_SIZE 128
#define DEFAULT_TTL            2

struct net_hdr {
        uint8_t  src_id;       /* Origin node ID */
        uint8_t  last_hop_id;  /* Node ID that broadcasted this hop */
        uint8_t  ttl;          /* Hops remaining */
        uint16_t seq;          /* Sequence counter from source */
} __attribute__((packed));

struct net_packet {
        struct net_hdr hdr;
        uint8_t audio[NET_AUDIO_PAYLOAD_SIZE];
} __attribute__((packed));

#define NET_PACKET_SIZE sizeof(struct net_packet)

void network_init(void);
bool network_is_duplicate(uint8_t src_id, uint16_t seq);

#endif /* NETWORK_H */


