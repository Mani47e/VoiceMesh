#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include "audio/audio.h"
#include "codec/codec.h"
#include "transport/transport.h"
#include "ringbuffer/ringbuffer.h"
#include "app/appstate.h"

#define BUTTON_NODE             DT_ALIAS(sw0)
#define BUTTON_POLL_MS          20
#define BUTTON_DEBOUNCE_SAMPLES 3

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);
static uint8_t seq_counter = 0;

/* Callback executed when RF packets arrive in PRX mode */
static void on_transport_rx(uint8_t seq, const uint8_t *data, size_t len)
{
        appstate_verify_seq(seq);

        int before = ring_available();
        for (size_t i = 0; i < len; i++) {
                ring_push(data[i]);
        }
        int after = ring_available();
        int pushed = after - before;

        if (pushed < (int)len) {
                appstate_add_ring_drops(len - pushed);
        }
}

/* ---------------- Capture / TX Thread ---------------- */
static void capture_thread_fn(void *p1, void *p2, void *p3)
{
        ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

        appstate_wait_threads_ready();
        printk("Capture thread started\n");

        static int16_t shrunk[AUDIO_SAMPLES_PER_BLOCK];

        while (1) {
                void *read_buf;
                size_t read_size;

                int r = audio_read(&read_buf, &read_size);
                if (r) {
                        printk("i2s_read failed: %d\n", r);
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
                for (int c = 0; c < CHUNKS_PER_BLOCK; c++) {
                        if (appstate_get() != DEV_STATE_TALK) {
                                break;
                        }

                        while (!transport_is_tx_ready() && appstate_get() == DEV_STATE_TALK) {
                                k_yield();
                        }
                        if (appstate_get() != DEV_STATE_TALK) {
                                break;
                        }

                        transport_send_chunk(seq_counter++, p + (c * ESB_CHUNK_DATA_SIZE), ESB_CHUNK_DATA_SIZE);
                }
        }
}

/* ---------------- Playback / RX Thread ---------------- */
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
                        printk("i2s_write failed: %d\n", w);
                        audio_free_tx(buf);
                }

                appstate_print_debug(ring_available());
        }
}

K_THREAD_DEFINE(capture_tid, 2048, capture_thread_fn, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(playback_tid, 2048, playback_thread_fn, NULL, NULL, NULL, 6, 0, 0);

/* ---------------- Main State Machine ---------------- */
int main(void)
{
        k_msleep(1000);
        printk("WALKIE START\n");

        appstate_init();
        ring_init();

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

        int err = transport_init(on_transport_rx);
        printk("ESB init (PRX) result: %d\n", err);
        if (err) {
                return 0;
        }

        audio_preload_silence();

        int ret = audio_start();
        printk("I2S BOTH trigger: %d\n", ret);
        if (ret) {
                return 0;
        }

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
                                        printk("Button pressed -> TALK\n");
                                        int e = transport_switch_mode(true);
                                        if (e) {
                                                printk("ESB switch to PTX failed: %d\n", e);
                                        } else {
                                                seq_counter = 0;
                                                transport_reset_tx_ready();
                                                appstate_set(DEV_STATE_TALK);
                                        }
                                } else {
                                        printk("Button released -> LISTEN\n");
                                        appstate_set(DEV_STATE_LISTEN);
                                        int e = transport_switch_mode(false);
                                        if (e) {
                                                printk("ESB switch to PRX failed: %d\n", e);
                                        }
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