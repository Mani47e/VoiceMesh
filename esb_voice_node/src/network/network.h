#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NET_AUDIO_PAYLOAD_SIZE 128
#define BROADCAST_NODE_ID      0xFF

struct net_hdr {
        uint8_t  src_id;       /* Originator node ID */
        uint8_t  dest_id;      /* Destination node ID (or 0xFF) */
        uint8_t  last_hop_id;  /* ID of the node that sent this hop */
        uint8_t  ttl;          /* Remaining hops allowed */
        uint16_t seq;          /* Unique packet sequence number */
} __attribute__((packed));

struct net_packet {
        struct net_hdr hdr;
        uint8_t audio[NET_AUDIO_PAYLOAD_SIZE];
} __attribute__((packed));

#define NET_PACKET_SIZE sizeof(struct net_packet)

void network_init(void);
bool network_is_duplicate(uint8_t src_id, uint16_t seq);

#endif /* NETWORK_H */