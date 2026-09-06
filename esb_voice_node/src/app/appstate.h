#ifndef APPSTATE_H
#define APPSTATE_H

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
        DEV_STATE_LISTEN = 0,
        DEV_STATE_TALK,
} dev_state_t;

void appstate_init(void);
dev_state_t appstate_get(void);
void appstate_set(dev_state_t state);

void appstate_wait_threads_ready(void);
void appstate_signal_threads_ready(void);

/* Metrics and Diagnostics */
void appstate_inc_esb_sent(void);
void appstate_inc_esb_failed(void);
void appstate_inc_esb_recv(void);
void appstate_inc_seq_gaps(void);
void appstate_add_ring_drops(uint32_t drops);
void appstate_inc_play_blocks(void);
void appstate_set_mic_peak(int32_t peak);
void appstate_verify_seq(uint8_t seq);
void appstate_reset_seq_tracking(void);
void appstate_print_debug(int ring_available_bytes);

#endif /* APPSTATE_H */