/*
 * ESB PTX (transmitter) test.
 *
 * Periodically sends a short text message. Whatever text the PRX
 * queues as its ACK payload comes back through the same event
 * handler as a normal RX event, so this side both sends and
 * receives real text.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <esb.h>
#include <string.h>
#include <stdio.h>

#define MAX_TEXT_LEN 32

static bool ready = true;
static struct esb_payload rx_payload;
static struct esb_payload tx_payload;

static void event_handler(struct esb_evt const *event)
{
	ready = true;

	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		printk("TX success (%u attempts)", event->tx_attempts);
		break;
	case ESB_EVENT_TX_FAILED:
		printk("TX failed -- is the PRX powered on and in range?");
		break;
	case ESB_EVENT_RX_RECEIVED:
		/* Any ACK payload queued by the PRX arrives here */
		while (esb_read_rx_payload(&rx_payload) == 0) {
			char text[MAX_TEXT_LEN + 1] = {0};
			int len = MIN(rx_payload.length, MAX_TEXT_LEN);

			memcpy(text, rx_payload.data, len);
			printk("RX (from PRX): %s\n", text);
		}
		break;
	}
}

static int esb_initialize(void)
{
	int err;
	uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
	uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
	uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
	struct esb_config config = ESB_DEFAULT_CONFIG;

	config.protocol = ESB_PROTOCOL_ESB_DPL;
	config.bitrate = ESB_BITRATE_2MBPS;
	config.event_handler = event_handler;
	config.mode = ESB_MODE_PTX;
	config.selective_auto_ack = false;

	err = esb_init(&config);
	if (err) {
		return err;
	}

	err = esb_set_base_address_0(base_addr_0);
	if (err) {
		return err;
	}

	err = esb_set_base_address_1(base_addr_1);
	if (err) {
		return err;
	}

	return esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
}

int main(void)
{
	int err = esb_initialize();

	if (err) {
		printk("ESB init failed: %d", err);
		return 0;
	}

	printk("ESB PTX test ready");

	int counter = 0;

	while (1) {
		if (ready) {
			ready = false;

			char msg[MAX_TEXT_LEN];
			int len = snprintf(msg, sizeof(msg), "PTX #%d", counter++);

			esb_flush_tx();

			tx_payload.pipe = 0;
			tx_payload.noack = false;
			tx_payload.length = MIN(len, MAX_TEXT_LEN);
			memcpy(tx_payload.data, msg, tx_payload.length);

			err = esb_write_payload(&tx_payload);
			if (err) {
				printk("Payload write failed: %d", err);
			} else {
				printk("TX: %s\n", msg);
			}
		}

		k_sleep(K_MSEC(500));
	}

	return 0;
}