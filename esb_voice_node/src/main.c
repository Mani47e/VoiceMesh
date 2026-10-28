#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#//include <zephyr/random/random.h>
#include <string.h>

#include "audio.h"
#include "codec.h"
#include "transport.h"
#include "ringbuffer.h"
#include "appstate.h"
#include "network.h"

/* ---------------- Node Configuration ---------------- */
/* Change MY_NODE_ID for each device: 1 (A), 2 (B), or 3 (C) */
#define MY_NODE_ID 3

#if MY_NODE_ID == 1
  #define TARGET_1_ID  2   /* Node B (1 hop away) */
  #define TARGET_1_TTL 1   /* Direct: No relaying needed */
  #define TARGET_2_ID  3   /* Node C (2 hops away) */
  #define TARGET_2_TTL 2   /* Multi-hop: Node B will relay */
#elif MY_NODE_ID == 2
  #define TARGET_1_ID  1   /* Node A (1 hop away) */
  #define TARGET_1_TTL 1
  #define TARGET_2_ID  3   /* Node C (1 hop away) */
  #define TARGET_2_TTL 1
#elif MY_NODE_ID == 3
  #define TARGET_1_ID  1   /* Node A (2 hops away) */
  #define TARGET_1_TTL 2   /* Multi-hop: Node B will relay */
  #define TARGET_2_ID  2   /* Node B (1 hop away) */
  #define TARGET_2_TTL 1
#endif

/*
 * BENCHTOP DISTANCE FILTER:
 * Prevents Node 1 and Node 3 from hearing each other directly on a desk.
 * Forces Node 1 <-> Node 3 packets to travel through Relay Node 2.
 */
#define SIMULATE_BENCH_DISTANCE 1

/* Button Devicetree Mappings */
#define BTN_BCAST_NODE DT_ALIAS(sw0)  /* Built-in Button (P1.06) */
#define BTN_TGT1_NODE  DT_ALIAS(sw1)  /* Ext Button 1 (P0.17)    */
#define BTN_TGT2_NODE  DT_ALIAS(sw2)  /* Ext Button 2 (P0.20)    */

static const struct gpio_dt_spec btn_bcast = GPIO_DT_SPEC_GET(BTN_BCAST_NODE, gpios);
static const struct gpio_dt_spec btn_tgt1  = GPIO_DT_SPEC_GET_OR(BTN_TGT1_NODE, gpios, {0});
static const struct gpio_dt_spec btn_tgt2  = GPIO_DT_SPEC_GET_OR(BTN_TGT2_NODE, gpios, {0});

/* Relay LED (LD1: P0.06) */
#define RELAY_LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec relay_led = GPIO_DT_SPEC_GET_OR(RELAY_LED_NODE, gpios, {0});

#define BUTTON_POLL_MS 20

static volatile uint8_t active_dest_id = BROADCAST_NODE_ID;
static volatile uint8_t active_tx_ttl  = 1;
static uint16_t seq_counter = 0;

K_MSGQ_DEFINE(relay_msgq, sizeof(struct net_packet), 8, 4);

/* ---------------- Network RX Routing Callback ---------------- */
static void on_network_rx(const struct net_packet *pkt)
{
        /* 1. Never process our own transmitted packets */
        if (pkt->hdr.src_id == MY_NODE_ID) {
                return;
        }

#if SIMULATE_BENCH_DISTANCE
        /* Artificial distance: Drop direct Node 1 <-> Node 3 link */
        if (MY_NODE_ID == 1 && pkt->hdr.last_hop_id == 3) return;
        if (MY_NODE_ID == 3 && pkt->hdr.last_hop_id == 1) return;
#endif

        /* 2. Deduplication: Prevents double audio playback and infinite loops */
        if (network_is_duplicate(pkt->hdr.src_id, pkt->hdr.seq)) {
                return;
        }

        bool is_for_me = (pkt->hdr.dest_id == MY_NODE_ID);
        bool is_bcast  = (pkt->hdr.dest_id == BROADCAST_NODE_ID);

        /* 3. Audio Playback: Play ONLY if targeted to this node or broadcast */
        if (is_for_me || is_bcast) {
                appstate_verify_seq((uint8_t)pkt->hdr.seq);

                int before = ring_available();
                for (size_t i = 0; i < NET_AUDIO_PAYLOAD_SIZE; i++) {
                        ring_push(pkt->audio[i]);
                }
                int after = ring_available();
                int pushed = after - before;

                if (pushed < (int)NET_AUDIO_PAYLOAD_SIZE) {
                        appstate_add_ring_drops(NET_AUDIO_PAYLOAD_SIZE - pushed);
                }
        }

        /* 4. Relay Forwarding:
         * Never relay if:
         * - Packet was addressed to this node (journey is complete)
         * - No hops remaining (ttl <= 1)
         * - This node is currently transmitting
         */
        if (!is_for_me && pkt->hdr.ttl > 1 && appstate_get() == DEV_STATE_LISTEN) {
                struct net_packet fwd = *pkt;
                fwd.hdr.ttl--;
                fwd.hdr.last_hop_id = MY_NODE_ID;
                k_msgq_put(&relay_msgq, &fwd, K_NO_WAIT);
        }
}

/* ---------------- Relay Worker Thread ---------------- */
static void relay_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Relay worker active on Node %d\n", MY_NODE_ID);

        struct net_packet fwd_pkt;

        while (1) {
                if (k_msgq_get(&relay_msgq, &fwd_pkt, K_FOREVER) == 0) {
                        if (appstate_get() != DEV_STATE_LISTEN) {
                                continue;
                        }

                       /* Hardware cycle counter jitter (500us to 2500us) */
						uint32_t jitter_us = 500 + (k_cycle_get_32() % 2000);
						k_busy_wait(jitter_us);

                        if (appstate_get() != DEV_STATE_LISTEN) {
                                continue;
                        }

                        transport_switch_mode(true);
                        transport_send_packet(&fwd_pkt);

                        while (!transport_is_tx_ready()) {
                                k_yield();
                        }

                        transport_switch_mode(false);

                        /* Toggle LED indicator on relay */
                        if (relay_led.port) {
                                gpio_pin_toggle_dt(&relay_led);
                        }
                }
        }
}

/* ---------------- Audio Capture / Local TX Thread ---------------- */
static void capture_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Capture worker active\n");

        static int16_t shrunk[AUDIO_SAMPLES_PER_BLOCK];
        static const int chunks_per_block = AUDIO_SAMPLES_PER_BLOCK / (NET_AUDIO_PAYLOAD_SIZE / sizeof(int16_t));

        while (1) {
                void *read_buf;
                size_t read_size;

                int r = audio_read(&read_buf, &read_size);
                if (r) {
                        k_msleep(5);
                        continue;
                }

                if (appstate_get() != DEV_STATE_TALK) {
                        audio_free_rx(read_buf);
                        continue;
                }

                int32_t peak = 0;
                codec_process_mic_block((int32_t *)read_buf, shrunk, AUDIO_SAMPLES_PER_BLOCK, &peak);
                appstate_set_mic_peak(peak);
                audio_free_rx(read_buf);

                uint8_t *p = (uint8_t *)shrunk;
                for (int c = 0; c < chunks_per_block; c++) {
                        if (appstate_get() != DEV_STATE_TALK) {
                                break;
                        }

                        while (!transport_is_tx_ready() && appstate_get() == DEV_STATE_TALK) {
                                k_yield();
                        }
                        if (appstate_get() != DEV_STATE_TALK) {
                                break;
                        }

                        struct net_packet pkt;
                        pkt.hdr.src_id = MY_NODE_ID;
                        pkt.hdr.dest_id = active_dest_id;
                        pkt.hdr.last_hop_id = MY_NODE_ID;
                        pkt.hdr.ttl = active_tx_ttl;
                        pkt.hdr.seq = seq_counter++;
                        memcpy(pkt.audio, p + (c * NET_AUDIO_PAYLOAD_SIZE), NET_AUDIO_PAYLOAD_SIZE);

                        transport_send_packet(&pkt);
                }
        }
}

/* ---------------- Audio Playback / Local RX Thread ---------------- */
static void playback_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Playback worker active\n");

        while (1) {
                void *buf;
                if (audio_alloc_tx(&buf) != 0) {
                        continue;
                }

                int32_t *out = (int32_t *)buf;

                if (appstate_get() == DEV_STATE_TALK) {
                        memset(buf, 0, AUDIO_BLOCK_SIZE);
                } else {
                        int avail = ring_available();
                        int needed = AUDIO_SAMPLES_PER_BLOCK * (int)sizeof(int16_t);

                        if (avail >= needed) {
                                for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
                                        int16_t sample;
                                        uint8_t *sb = (uint8_t *)&sample;
                                        ring_pop(&sb[0]);
                                        ring_pop(&sb[1]);
                                        out[i] = codec_expand_sample(sample);
                                }
                                appstate_inc_play_blocks();
                        } else {
                                memset(buf, 0, AUDIO_BLOCK_SIZE);
                        }
                }

                int w = audio_write(buf, AUDIO_BLOCK_SIZE);
                if (w) {
                        audio_free_tx(buf);
                }

                appstate_print_debug(ring_available());
        }
}

K_THREAD_DEFINE(capture_tid,  2048, capture_thread_fn,  NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(playback_tid, 2048, playback_thread_fn, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(relay_tid,    2048, relay_thread_fn,    NULL, NULL, NULL, 5, 0, 0);

/* ---------------- Main Control Loop ---------------- */
int main(void)
{
        k_msleep(1000);
        printk("=== VOICE NODE %d READY ===\n", MY_NODE_ID);

        appstate_init();
        ring_init();
        network_init();

        if (audio_init()) return 0;

        /* Built-in button */
        if (!gpio_is_ready_dt(&btn_bcast)) return 0;
        gpio_pin_configure_dt(&btn_bcast, GPIO_INPUT);

        /* External Button 1 */
        if (btn_tgt1.port && gpio_is_ready_dt(&btn_tgt1)) {
                gpio_pin_configure_dt(&btn_tgt1, GPIO_INPUT);
        }

        /* External Button 2 */
        if (btn_tgt2.port && gpio_is_ready_dt(&btn_tgt2)) {
                gpio_pin_configure_dt(&btn_tgt2, GPIO_INPUT);
        }

        /* Relay Status LED */
        if (relay_led.port && gpio_is_ready_dt(&relay_led)) {
                gpio_pin_configure_dt(&relay_led, GPIO_OUTPUT_INACTIVE);
        }

        if (transport_init(on_network_rx)) return 0;

        audio_preload_silence();
        if (audio_start()) return 0;

        /* Wake up all 3 worker threads */
        appstate_signal_threads_ready();

        uint8_t last_target = 0;

        while (1) {
                bool bcast_pressed = gpio_pin_get_dt(&btn_bcast) > 0;
                bool tgt1_pressed  = (btn_tgt1.port) ? (gpio_pin_get_dt(&btn_tgt1) > 0) : false;
                bool tgt2_pressed  = (btn_tgt2.port) ? (gpio_pin_get_dt(&btn_tgt2) > 0) : false;

                uint8_t current_target = 0;
                uint8_t current_ttl = 1;

                if (bcast_pressed) {
                        current_target = BROADCAST_NODE_ID;
                        current_ttl = 2; /* Broadcast traverses multi-hop */
                } else if (tgt1_pressed) {
                        current_target = TARGET_1_ID;
                        current_ttl = TARGET_1_TTL;
                } else if (tgt2_pressed) {
                        current_target = TARGET_2_ID;
                        current_ttl = TARGET_2_TTL;
                }

                if (current_target != last_target) {
                        last_target = current_target;

                        if (current_target != 0) {
                                printk("TALK -> Target %d (TTL=%d)\n", current_target, current_ttl);
                                active_dest_id = current_target;
                                active_tx_ttl  = current_ttl;
                                if (transport_switch_mode(true) == 0) {
                                        seq_counter = 0;
                                        transport_reset_tx_ready();
                                        appstate_set(DEV_STATE_TALK);
                                }
                        } else {
                                printk("STOP -> Mode LISTEN\n");
                                appstate_set(DEV_STATE_LISTEN);
                                transport_switch_mode(false);
                                appstate_reset_seq_tracking();
                                ring_reset();
                        }
                }

                k_msleep(BUTTON_POLL_MS);
        }

        return 0;
}