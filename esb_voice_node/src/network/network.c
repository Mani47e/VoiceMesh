#include "network.h"

#define DEDUP_HISTORY_SIZE 32

struct packet_sig {
        uint8_t  src_id;
        uint16_t seq;
};

static struct packet_sig history[DEDUP_HISTORY_SIZE];
static int history_idx = 0;

void network_init(void)
{
        for (int i = 0; i < DEDUP_HISTORY_SIZE; i++) {
                history[i].src_id = 0xFF;
                history[i].seq = 0xFFFF;
        }
        history_idx = 0;
}

bool network_is_duplicate(uint8_t src_id, uint16_t seq)
{
        for (int i = 0; i < DEDUP_HISTORY_SIZE; i++) {
                if (history[i].src_id == src_id && history[i].seq == seq) {
                        return true; /* Drop duplicate */
                }
        }

        history[history_idx].src_id = src_id;
        history[history_idx].seq = seq;
        history_idx = (history_idx + 1) % DEDUP_HISTORY_SIZE;

        return false;
}