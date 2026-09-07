#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "audio.h"
#include "codec.h"
#include "transport.h"
#include "ringbuffer.h"
#include "appstate.h"
#include "network.h"

/* ---------------- Node Configuration ---------------- */
/* Flash each device with its unique ID: 1, 2, or 3 */
#define MY_NODE_ID             3

/* Set to 1 ONLY on Node 3 to simulate benchtop distance */
#define SIMULATE_BENCH_DISTANCE 1

#define BUTTON_NODE             DT_ALIAS(sw0)
#define BUTTON_POLL_MS          20
#define BUTTON_DEBOUNCE_SAMPLES 3

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static uint16_t seq_counter = 0;

#define RELAY_LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec relay_led = GPIO_DT_SPEC_GET_OR(RELAY_LED_NODE, gpios, {0});

/* Queue for packets waiting to be relayed forward */
K_MSGQ_DEFINE(relay_msgq, sizeof(struct net_packet), 8, 4);

/* ---------------- Network RX Callback ---------------- */
static void on_network_rx(const struct net_packet *pkt)
{
        /* 1. Never process our own transmitted packets */
        if (pkt->hdr.src_id == MY_NODE_ID) {
                return;
        }

#if SIMULATE_BENCH_DISTANCE
        /* Simulated Range: Drop direct packets from Node 1 (must arrive via Relay Node 2) */
        if (pkt->hdr.last_hop_id == 1) {
                return;
        }
#endif

        /* 2. Deduplication check */
        if (network_is_duplicate(pkt->hdr.src_id, pkt->hdr.seq)) {
                return;
        }

        appstate_verify_seq((uint8_t)pkt->hdr.seq);

        /* 3. Send audio to local speaker */
        int before = ring_available();
        for (size_t i = 0; i < NET_AUDIO_PAYLOAD_SIZE; i++) {
                ring_push(pkt->audio[i]);
        }
        int after = ring_available();
        int pushed = after - before;

        if (pushed < (int)NET_AUDIO_PAYLOAD_SIZE) {
                appstate_add_ring_drops(NET_AUDIO_PAYLOAD_SIZE - pushed);
        }

        /* 4. Multi-hop relay decision */
        /* Only forward if hops remain, TTL > 1, and this device is strictly configured as a relay */
        if (pkt->hdr.ttl > 1 && appstate_get() == DEV_STATE_LISTEN) {
                struct net_packet fwd = *pkt;
                fwd.hdr.ttl--;
                fwd.hdr.last_hop_id = MY_NODE_ID;
                k_msgq_put(&relay_msgq, &fwd, K_NO_WAIT);
        }
}

/* ---------------- Relay Forwarding Worker Thread ---------------- */
static void relay_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Relay thread started (Node ID: %d)\n", MY_NODE_ID);

        struct net_packet fwd_pkt;

        while (1) {
                if (k_msgq_get(&relay_msgq, &fwd_pkt, K_FOREVER) == 0) {
                        if (appstate_get() != DEV_STATE_LISTEN) {
                                continue;
                        }

                        /* Switch to PTX mode, blast repeated chunk, return to PRX */
                        transport_switch_mode(true);
                        transport_send_packet(&fwd_pkt);

                        while (!transport_is_tx_ready()) {
                                k_yield();
                        }

                        transport_switch_mode(false);

                        /* Toggle LED to indicate active forwarding */
                        if (relay_led.port) {
                                gpio_pin_toggle_dt(&relay_led);
                        }
                }
        }
}

/* ---------------- Capture / Local TX Thread ---------------- */
static void capture_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Capture thread started\n");

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
                        pkt.hdr.last_hop_id = MY_NODE_ID;
                        pkt.hdr.ttl = DEFAULT_TTL;
                        pkt.hdr.seq = seq_counter++;
                        memcpy(pkt.audio, p + (c * NET_AUDIO_PAYLOAD_SIZE), NET_AUDIO_PAYLOAD_SIZE);

                        transport_send_packet(&pkt);
                }
        }
}

/* ---------------- Playback / Local RX Thread ---------------- */
static void playback_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Playback thread started\n");

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

/* ---------------- Main State Machine ---------------- */
int main(void)
{
        k_msleep(1000);
        printk("=== WALKIE START (Node %d) ===\n", MY_NODE_ID);

        appstate_init();
        ring_init();
        network_init();

        if (audio_init()) {
                return 0;
        }

        if (!gpio_is_ready_dt(&button)) {
                printk("Button GPIO not ready\n");
                return 0;
        }
        if (gpio_pin_configure_dt(&button, GPIO_INPUT)) {
                printk("Button GPIO configure failed\n");
                return 0;
        }

		/* Configure Relay Status LED */
        if (relay_led.port && gpio_is_ready_dt(&relay_led)) {
                gpio_pin_configure_dt(&relay_led, GPIO_OUTPUT_INACTIVE);
        }

        int err = transport_init(on_network_rx);
        if (err) {
                printk("Transport init failed: %d\n", err);
                return 0;
        }

        audio_preload_silence();

        int ret = audio_start();
        if (ret) {
                printk("I2S trigger failed: %d\n", ret);
                return 0;
        }

        /* Wake up capture, playback, and relay threads */
        appstate_signal_threads_ready();

        bool was_pressed = false;
        int stable_count = 0;

        while (1) {
                bool pressed = gpio_pin_get_dt(&button) > 0;

                if (pressed != was_pressed) {
                        stable_count++;
                        if (stable_count >= BUTTON_DEBOUNCE_SAMPLES) {
                                was_pressed = pressed;
                                stable_count = 0;

                                if (pressed) {
                                        printk("Button -> TALK\n");
                                        int e = transport_switch_mode(true);
                                        if (e == 0) {
                                                seq_counter = 0;
                                                transport_reset_tx_ready();
                                                appstate_set(DEV_STATE_TALK);
                                        }
                                } else {
                                        printk("Button -> LISTEN\n");
                                        appstate_set(DEV_STATE_LISTEN);
                                        transport_switch_mode(false);
                                        appstate_reset_seq_tracking();
                                        ring_reset();
                                }
                        }
                } else {
                        stable_count = 0;
                }

                k_msleep(BUTTON_POLL_MS);
        }

        return 0;
}