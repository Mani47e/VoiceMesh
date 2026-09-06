#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ESB_CHUNK_DATA_SIZE      128
#define SAMPLES_PER_CHUNK        (ESB_CHUNK_DATA_SIZE / sizeof(int16_t))       /* 64 */
#define CHUNKS_PER_BLOCK         (512 / SAMPLES_PER_CHUNK)                    /* 8  */

struct chunk_hdr {
        uint8_t seq;
        uint8_t reserved;
};

#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE)

typedef void (*transport_rx_callback_t)(uint8_t seq, const uint8_t *data, size_t len);

int transport_init(transport_rx_callback_t rx_cb);
int transport_switch_mode(bool ptx);
bool transport_is_tx_ready(void);
void transport_reset_tx_ready(void);
int transport_send_chunk(uint8_t seq, const uint8_t *data, size_t len);

#endif /* TRANSPORT_H */