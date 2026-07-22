/*
 * ESB PRX (receiver) test.
 *
 * Listens continuously. Every time it gets a packet it prints the
 * text and queues its own text reply, which goes out automatically
 * on the next ACK -- so this side both receives and sends real text,
 * just piggybacked on the ACK rather than sent on its own schedule.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <esb.h>
#include <string.h>
#include <stdio.h>

#define MAX_TEXT_LEN 32

static struct esb_payload rx_payload;
static int reply_counter;

static void queue_reply(void)
{
	struct esb_payload tx_payload = {0};
	char msg[MAX_TEXT_LEN];
	int len = snprintf(msg, sizeof(msg), "Nigga #%d", reply_counter++);

	tx_payload.pipe = 0;
	tx_payload.length = MIN(len, MAX_TEXT_LEN);
	memcpy(tx_payload.data, msg, tx_payload.length);

	esb_write_payload(&tx_payload); /* rides out on the next ACK */
	printk("TX (via ACK): %s\n", msg);
}

static void event_handler(struct esb_evt const *event)
{
	switch (event->evt_id) {
	case ESB_EVENT_TX_SUCCESS:
		printk("ACK payload delivered");
		break;
	case ESB_EVENT_TX_FAILED:
		printk("ACK payload not delivered");
		break;
	case ESB_EVENT_RX_RECEIVED:
		while (esb_read_rx_payload(&rx_payload) == 0) {
			char text[MAX_TEXT_LEN + 1] = {0};
			int len = MIN(rx_payload.length, MAX_TEXT_LEN);

			memcpy(text, rx_payload.data, len);
			printk("RX (from PTX): %s\n", text);

			queue_reply();
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
	config.mode = ESB_MODE_PRX;
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

	err = esb_start_rx();
	if (err) {
		printk("esb_start_rx failed: %d", err);
		return 0;
	}

	printk("ESB PRX test ready, listening...");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}