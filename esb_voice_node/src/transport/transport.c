#include "transport.h"
#include "appstate.h"
#include <esb.h>
#include <zephyr/sys/util.h>
#include <string.h>

static const uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
static const uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
static const uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};

static struct esb_payload rx_payload;
static struct esb_payload tx_payload;
static volatile bool tx_ready = true;
static transport_rx_callback_t rx_callback = NULL;

static void on_esb_event(struct esb_evt const *event)
{
        switch (event->evt_id) {
        case ESB_EVENT_TX_SUCCESS:
                tx_ready = true;
                break;

        case ESB_EVENT_TX_FAILED:
                tx_ready = true;
                appstate_inc_esb_failed();
                break;

        case ESB_EVENT_RX_RECEIVED:
                while (esb_read_rx_payload(&rx_payload) == 0) {
                        if (appstate_get() != DEV_STATE_LISTEN) {
                                continue;
                        }
                        if (rx_payload.length != NET_PACKET_SIZE) {
                                continue;
                        }

                        if (rx_callback) {
                                rx_callback((const struct net_packet *)rx_payload.data);
                        }
                }
                break;
        }
}

static int esb_apply_addresses(void)
{
        int err = esb_set_base_address_0((uint8_t *)base_addr_0);
        if (err) return err;
        err = esb_set_base_address_1((uint8_t *)base_addr_1);
        if (err) return err;
        err = esb_set_prefixes((uint8_t *)addr_prefix, ARRAY_SIZE(addr_prefix));
        if (err) return err;
        return esb_set_rf_channel(2);
}

int transport_init(transport_rx_callback_t rx_cb)
{
        rx_callback = rx_cb;
        return transport_switch_mode(false);
}

int transport_switch_mode(bool ptx)
{
        esb_disable();

        struct esb_config config = ESB_DEFAULT_CONFIG;
        config.protocol = ESB_PROTOCOL_ESB_DPL;
        config.bitrate = ESB_BITRATE_2MBPS;
        config.event_handler = on_esb_event;
        config.mode = ptx ? ESB_MODE_PTX : ESB_MODE_PRX;
        config.selective_auto_ack = false;

        int err = esb_init(&config);
        if (err) return err;

        err = esb_apply_addresses();
        if (err) return err;

        if (!ptx) {
                err = esb_start_rx();
                if (err) return err;
        }

        return 0;
}

bool transport_is_tx_ready(void)
{
        return tx_ready;
}

void transport_reset_tx_ready(void)
{
        tx_ready = true;
}

int transport_send_packet(const struct net_packet *pkt)
{
        tx_payload.length = NET_PACKET_SIZE;
        memcpy(tx_payload.data, pkt, NET_PACKET_SIZE);

        tx_ready = false;
        int err = esb_write_payload(&tx_payload);
        if (err) {
                esb_flush_tx();
                tx_ready = true;
                appstate_inc_esb_failed();
                return err;
        }

        appstate_inc_esb_sent();
        return 0;
}