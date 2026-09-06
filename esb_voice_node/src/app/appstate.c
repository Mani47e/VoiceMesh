#include "appstate.h"
#include <zephyr/sys/printk.h>

static atomic_t current_state = ATOMIC_INIT(DEV_STATE_LISTEN);
K_SEM_DEFINE(threads_ready_sem, 0, 2);

static uint32_t dbg_esb_sent, dbg_esb_failed, dbg_esb_recv;
static uint32_t dbg_ring_drops, dbg_play_blocks, dbg_seq_gaps;
static int32_t  dbg_mic_peak;
static uint8_t  last_seq;
static bool     have_last_seq;

void appstate_init(void)
{
        atomic_set(&current_state, DEV_STATE_LISTEN);
        have_last_seq = false;
}

dev_state_t appstate_get(void)
{
        return (dev_state_t)atomic_get(&current_state);
}

void appstate_set(dev_state_t state)
{
        atomic_set(&current_state, state);
}

void appstate_wait_threads_ready(void)
{
        k_sem_take(&threads_ready_sem, K_FOREVER);
}

void appstate_signal_threads_ready(void)
{
        k_sem_give(&threads_ready_sem);
        k_sem_give(&threads_ready_sem);
}

void appstate_inc_esb_sent(void)    { dbg_esb_sent++; }
void appstate_inc_esb_failed(void)  { dbg_esb_failed++; }
void appstate_inc_esb_recv(void)    { dbg_esb_recv++; }
void appstate_inc_seq_gaps(void)    { dbg_seq_gaps++; }
void appstate_add_ring_drops(uint32_t drops) { dbg_ring_drops += drops; }
void appstate_inc_play_blocks(void) { dbg_play_blocks++; }

void appstate_set_mic_peak(int32_t peak)
{
        if (peak > dbg_mic_peak) {
                dbg_mic_peak = peak;
        }
}

void appstate_verify_seq(uint8_t seq)
{
        if (have_last_seq && (uint8_t)(seq - last_seq) != 1) {
                dbg_seq_gaps++;
        }
        last_seq = seq;
        have_last_seq = true;
        dbg_esb_recv++;
}

void appstate_reset_seq_tracking(void)
{
        have_last_seq = false;
}

void appstate_print_debug(int ring_available_bytes)
{
        static int64_t last;
        int64_t now = k_uptime_get();
        if (now - last < 2000) {
                return;
        }
        last = now;

        if (appstate_get() == DEV_STATE_TALK) {
                printk("[TX] mic_peak=%d sent=%u failed=%u\n",
                       dbg_mic_peak, dbg_esb_sent, dbg_esb_failed);
        } else {
                printk("[RX] recv=%u seq_gaps=%u ring_avail=%d drops=%u play=%u\n",
                       dbg_esb_recv, dbg_seq_gaps, ring_available_bytes,
                       dbg_ring_drops, dbg_play_blocks);
        }

        dbg_esb_sent = dbg_esb_failed = dbg_esb_recv = 0;
        dbg_ring_drops = dbg_play_blocks = dbg_seq_gaps = 0;
        dbg_mic_peak = 0;
}