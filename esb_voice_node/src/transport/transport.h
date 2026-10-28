#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include "network.h"

typedef void (*transport_rx_callback_t)(const struct net_packet *pkt);

int  transport_init(transport_rx_callback_t rx_cb);
int  transport_switch_mode(bool ptx);
bool transport_is_tx_ready(void);
void transport_reset_tx_ready(void);
int  transport_send_packet(const struct net_packet *pkt);

#endif /* TRANSPORT_H */