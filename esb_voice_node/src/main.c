/*
 * Default state: ESB in PRX mode, listening for audio and playing it
 *                 back through I2S TX.
 * Button held:   ESB switches to PTX mode, mic audio is captured via
 *                 I2S RX, chunked, and transmitted over ESB.
 * Button released: switches back to PRX/listening.
 *
 * I2S itself is configured once and kept running in I2S_DIR_BOTH for
 * the life of the program -- only what the RX/TX worker threads DO
 * with the buffers changes with state. This avoids re-triggering I2S
 * (which is fiddly) on every button press.
 */

 /* Always consumes I2S RX. In LISTEN state it just drains (keeps
 * I2S_DIR_BOTH alive). In TALK state it does gain + chunk + ESB send.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/atomic.h>
#include <string.h>

#include <esb.h>

//Config 

#define SAMPLE_RATE             8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT       4
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK)

#define MIC_GAIN                8

#define ESB_CHUNK_DATA_SIZE      128
#define SAMPLES_PER_CHUNK        (ESB_CHUNK_DATA_SIZE / sizeof(int16_t))       /* 64 */
#define CHUNKS_PER_BLOCK         (AUDIO_SAMPLES_PER_BLOCK / SAMPLES_PER_CHUNK) /* 8  */

#define BUTTON_NODE              DT_ALIAS(sw0)
#define BUTTON_POLL_MS           20
#define BUTTON_DEBOUNCE_SAMPLES  3

#define RING_SIZE 8000 

struct chunk_hdr {
	uint8_t seq;
	uint8_t reserved;
};
#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE) /* 130 */

/* ---------------- Devices ---------------- */

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE, gpios);

static struct i2s_config tx_cfg;
static struct i2s_config rx_cfg;

K_MEM_SLAB_DEFINE(tx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

//Shared state

typedef enum {
	DEV_STATE_LISTEN = 0, /* ESB PRX, playing back received audio */
	DEV_STATE_TALK,        /* ESB PTX, capturing mic and sending    */
} dev_state_t;

static atomic_t dev_state = ATOMIC_INIT(DEV_STATE_LISTEN);

K_SEM_DEFINE(threads_ready_sem, 0, 2);

//  ESB
static const uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
static const uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
static const uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

static struct esb_payload rx_payload;
static struct esb_payload tx_payload;
static volatile bool esb_ready = true;
static uint8_t seq_counter = 0;

// Playback ring buffer
/* Filled by the ESB RX event handler, drained by the playback thread. */

static uint8_t ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

static inline int ring_available(void)
{
	return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

static inline void ring_push(uint8_t b)
{
	int next = (ring_head + 1) % RING_SIZE;
	if (next != ring_tail) {
		ring[ring_head] = b;
		ring_head = next;
	}
	/* else: ring full, byte dropped -- counted by caller */
}

static inline void ring_reset(void)
{
	ring_head = 0;
	ring_tail = 0;
}

/* ---------------- Debug counters ---------------- */

static uint32_t dbg_esb_sent, dbg_esb_failed, dbg_esb_recv;
static uint32_t dbg_ring_drops, dbg_play_blocks, dbg_seq_gaps;
static int32_t  dbg_mic_peak;
static uint8_t  last_seq;
static bool     have_last_seq;

static void print_debug(void)
{
	static int64_t last;
	int64_t now = k_uptime_get();
	if (now - last < 2000) {
		return;
	}
	last = now;

	if (atomic_get(&dev_state) == DEV_STATE_TALK) {
		printk("[TX] mic_peak=%d sent=%u failed=%u\n",
		       dbg_mic_peak, dbg_esb_sent, dbg_esb_failed);
	} else {
		printk("[RX] recv=%u seq_gaps=%u ring_avail=%d drops=%u play=%u\n",
		       dbg_esb_recv, dbg_seq_gaps, ring_available(),
		       dbg_ring_drops, dbg_play_blocks);
	}

	dbg_esb_sent = dbg_esb_failed = dbg_esb_recv = 0;
	dbg_ring_drops = dbg_play_blocks = dbg_seq_gaps = 0;
	dbg_mic_peak = 0;
}

/* ---------------- ESB event handler ---------------- */

static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		esb_ready = true;
		break;

	case ESB_EVENT_TX_FAILED:
		esb_ready = true;
		dbg_esb_failed++;
		break;

	case ESB_EVENT_RX_RECEIVED:
		while (esb_read_rx_payload(&rx_payload) == 0) {
			if (atomic_get(&dev_state) != DEV_STATE_LISTEN) {
				continue; /* not expecting audio while talking */
			}
			if (rx_payload.length != CHUNK_PKT_SIZE) {
				continue;
			}

			struct chunk_hdr hdr;
			memcpy(&hdr, rx_payload.data, sizeof(hdr));

			if (have_last_seq && (uint8_t)(hdr.seq - last_seq) != 1) {
				dbg_seq_gaps++;
			}
			last_seq = hdr.seq;
			have_last_seq = true;
			dbg_esb_recv++;

			uint8_t *payload = rx_payload.data + sizeof(hdr);
			int before = ring_available();
			for (int i = 0; i < ESB_CHUNK_DATA_SIZE; i++) {
				ring_push(payload[i]);
			}
			int after = ring_available();
			int pushed = after - before;
			if (pushed < ESB_CHUNK_DATA_SIZE) {
				dbg_ring_drops += (ESB_CHUNK_DATA_SIZE - pushed);
			}
		}
		break;
	}
}

/* ---------------- ESB mode switching ---------------- */

static int esb_apply_addresses(void)
{
	int err = esb_set_base_address_0((uint8_t *)base_addr_0);
	if (err) {
		return err;
	}
	err = esb_set_base_address_1((uint8_t *)base_addr_1);
	if (err) {
		return err;
	}
	err = esb_set_prefixes((uint8_t *)addr_prefix, ARRAY_SIZE(addr_prefix));
	if (err) {
		return err;
	}
	return esb_set_rf_channel(2); /* must match on both devices */
}

/*
 * Fully re-initializes the ESB stack in the requested role.
 * Cost is a handful of ms -- fine for a button press/release edge,
 * not something you'd want to do per-packet.
 */
static int esb_switch_mode(bool ptx)
{
	int err;

	esb_disable();

	struct esb_config config = ESB_DEFAULT_CONFIG;
	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.event_handler = event_handler;
	config.mode = ptx ? ESB_MODE_PTX : ESB_MODE_PRX;
	config.selective_auto_ack = false;

	err = esb_init(&config);
	if (err) {
		return err;
	}

	err = esb_apply_addresses();
	if (err) {
		return err;
	}

	if (!ptx) {
		err = esb_start_rx();
		if (err) {
			return err;
		}
	}

	return 0;
}

/* ---------------- I2S setup ---------------- */

static int configure_i2s_rxntx(void)
{
	if (!device_is_ready(i2s_dev)) {
		printk("I2S not ready\n");
		return -ENODEV;
	}

	tx_cfg.word_size = AUDIO_WORD_SIZE;
	tx_cfg.channels = 1;
	tx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	tx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	tx_cfg.frame_clk_freq = SAMPLE_RATE;
	tx_cfg.mem_slab = &tx_mem_slab;
	tx_cfg.block_size = AUDIO_BLOCK_SIZE;
	tx_cfg.timeout = SYS_FOREVER_MS;

	int tx_ret = i2s_configure(i2s_dev, I2S_DIR_TX, &tx_cfg);
	printk("I2S TX configure: %d\n", tx_ret);
	if (tx_ret) {
		return tx_ret;
	}

	rx_cfg.word_size = AUDIO_WORD_SIZE;
	rx_cfg.channels = 1;
	rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
	rx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
	rx_cfg.frame_clk_freq = SAMPLE_RATE;
	rx_cfg.mem_slab = &rx_mem_slab;
	rx_cfg.block_size = AUDIO_BLOCK_SIZE;
	rx_cfg.timeout = SYS_FOREVER_MS;

	int rx_ret = i2s_configure(i2s_dev, I2S_DIR_RX, &rx_cfg);
	printk("I2S RX configure: %d\n", rx_ret);
	if (rx_ret) {
		return rx_ret;
	}

	return 0;
}

static void preload_silence(void)
{
	for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
		void *buf;
		k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER);
		memset(buf, 0, AUDIO_BLOCK_SIZE);
		int ret = i2s_write(i2s_dev, buf, AUDIO_BLOCK_SIZE);
		printk("Preload write %d = %d\n", i, ret);
	}
}

/* ---------------- Capture / TX worker ---------------- */
/*
 * Always consumes I2S RX. In LISTEN state it just drains (keeps
 * I2S_DIR_BOTH alive). In TALK state it does gain + chunk + ESB send.
 */
static void capture_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_take(&threads_ready_sem, K_FOREVER);
	printk("Capture thread started\n");

	static int16_t shrunk[AUDIO_SAMPLES_PER_BLOCK];

	while (1) {
		void *read_buf;
		size_t read_size;

		int r = i2s_read(i2s_dev, &read_buf, &read_size);
		if (r) {
			printk("i2s_read failed: %d\n", r);
			k_msleep(5);
			continue;
		}

		if (atomic_get(&dev_state) != DEV_STATE_TALK) {
			k_mem_slab_free(&rx_mem_slab, read_buf);
			continue;
		}

		int32_t *in = (int32_t *)read_buf;
		for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
			int32_t shifted = in[i] >> 16;
			int32_t boosted = shifted * MIC_GAIN;
			if (boosted > 32767) {
				boosted = 32767;
			}
			if (boosted < -32768) {
				boosted = -32768;
			}
			shrunk[i] = (int16_t)boosted;

			int32_t mag = boosted < 0 ? -boosted : boosted;
			if (mag > dbg_mic_peak) {
				dbg_mic_peak = mag;
			}
		}
		k_mem_slab_free(&rx_mem_slab, read_buf);

		uint8_t *p = (uint8_t *)shrunk;
		for (int c = 0; c < CHUNKS_PER_BLOCK; c++) {
			if (atomic_get(&dev_state) != DEV_STATE_TALK) {
				break; /* button released mid-block, stop immediately */
			}

			struct chunk_hdr hdr = { .seq = seq_counter++, .reserved = 0 };

			tx_payload.length = sizeof(hdr) + ESB_CHUNK_DATA_SIZE;
			memcpy(tx_payload.data, &hdr, sizeof(hdr));
			memcpy(tx_payload.data + sizeof(hdr),
			       p + (c * ESB_CHUNK_DATA_SIZE), ESB_CHUNK_DATA_SIZE);

			while (!esb_ready && atomic_get(&dev_state) == DEV_STATE_TALK) {
				k_yield();
			}
			if (atomic_get(&dev_state) != DEV_STATE_TALK) {
				break;
			}

			esb_ready = false;
			int werr = esb_write_payload(&tx_payload);
			if (werr) {
				esb_flush_tx();
				esb_ready = true;
				dbg_esb_failed++;
			} else {
				dbg_esb_sent++;
			}
		}
	}
}

/* ---------------- Playback / RX worker ---------------- */
/*
 * Always produces I2S TX. In LISTEN state it plays back audio pulled
 * from the ring buffer (silence if underrun). In TALK state it plays
 * silence so you don't hear your own room played back at you.
 */
static void playback_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	k_sem_take(&threads_ready_sem, K_FOREVER);
	printk("Playback thread started\n");

	while (1) {
		void *buf;
		if (k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER) != 0) {
			continue;
		}

		int32_t *out = (int32_t *)buf;

		if (atomic_get(&dev_state) == DEV_STATE_TALK) {
			memset(buf, 0, AUDIO_BLOCK_SIZE);
		} else {
			int avail = ring_available();
			int needed = AUDIO_SAMPLES_PER_BLOCK * (int)sizeof(int16_t);

			if (avail >= needed) {
				for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
					int16_t sample;
					uint8_t *sb = (uint8_t *)&sample;
					sb[0] = ring[ring_tail];
					ring_tail = (ring_tail + 1) % RING_SIZE;
					sb[1] = ring[ring_tail];
					ring_tail = (ring_tail + 1) % RING_SIZE;
					out[i] = ((int32_t)sample) << 16;
				}
				dbg_play_blocks++;
			} else {
				memset(buf, 0, AUDIO_BLOCK_SIZE);
			}
		}

		int w = i2s_write(i2s_dev, buf, AUDIO_BLOCK_SIZE);
		if (w) {
			printk("i2s_write failed: %d\n", w);
			k_mem_slab_free(&tx_mem_slab, buf);
		}

		print_debug();
	}
}

K_THREAD_DEFINE(capture_tid, 2048, capture_thread_fn, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(playback_tid, 2048, playback_thread_fn, NULL, NULL, NULL, 6, 0, 0);

/* ---------------- main: button state machine ---------------- */

int main(void)
{
	k_msleep(1000);
	printk("WALKIE START\n");

	if (configure_i2s_rxntx()) {
		return 0;
	}

	if (!gpio_is_ready_dt(&button)) {
		printk("button not ready\n");
		return 0;
	}
	if (gpio_pin_configure_dt(&button, GPIO_INPUT)) {
		printk("Button gpio configure failed\n");
		return 0;
	}

	/* Boot listening (PRX) */
	int err = esb_switch_mode(false);
	printk("ESB init (PRX) result: %d\n", err);
	if (err) {
		printk("ESB init failed: %d\n", err);
		return 0;
	}

	preload_silence();

	int ret = i2s_trigger(i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
	printk("I2S BOTH trigger: %d\n", ret);
	if (ret) {
		return 0;
	}

	/* Release both worker threads now that I2S + ESB are live */
	k_sem_give(&threads_ready_sem);
	k_sem_give(&threads_ready_sem);

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
					int e = esb_switch_mode(true);
					if (e) {
						printk("ESB switch to PTX failed: %d\n", e);
					} else {
						seq_counter = 0;
						esb_ready = true;
						atomic_set(&dev_state, DEV_STATE_TALK);
					}
				} else {
					printk("Button released -> LISTEN\n");
					atomic_set(&dev_state, DEV_STATE_LISTEN);
					int e = esb_switch_mode(false);
					if (e) {
						printk("ESB switch to PRX failed: %d\n", e);
					}
					have_last_seq = false;
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