#include "contiki.h"
#include "sys/etimer.h"
#include "audio.h"
#include <stdio.h>

PROCESS(loopback_process, "I2S loopback");
AUTOSTART_PROCESSES(&loopback_process);

PROCESS_THREAD(loopback_process, ev, data)
{
  static struct etimer et;
  static uint32_t n;

  PROCESS_BEGIN();

  etimer_set(&et, CLOCK_SECOND * 3);   /* let USB CDC enumerate */
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

  printf("LOOPBACK START (24-bit mono, %d Hz)\n", AUDIO_SAMPLE_RATE);
  printf("init -> %d\n", audio_init());
  printf("start -> %d\n", audio_start());

  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    printf("alive %lu cb=%lu rx=%ld %ld\n",
           (unsigned long)n++, (unsigned long)i2s_cb_count,
           (long)dbg_rx[0], (long)dbg_rx[1]);
  }

  PROCESS_END();
}