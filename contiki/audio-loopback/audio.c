#include "contiki.h"
#include "audio.h"
#include <nrfx_i2s.h>
#include <stdio.h>


volatile uint32_t i2s_cb_count;
volatile int32_t  dbg_rx[2];

static nrfx_i2s_t i2s_inst = NRFX_I2S_INSTANCE(0);
static nrfx_i2s_config_t i2s_cfg;

static uint32_t buf_rx[2][AUDIO_SAMPLES_PER_BLOCK];
static uint32_t buf_tx[2][AUDIO_SAMPLES_PER_BLOCK];
static uint8_t next_idx;

/* DC blocker state */
static int32_t dc_x1, dc_y1;

static void
audio_loopback_block(const uint32_t *rx, uint32_t *tx)
{
  int i;
  for(i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
    int32_t x = ((int32_t)(rx[i] << 8)) >> 8;       /* sign-extend 24-bit */
    int32_t y = x - dc_x1 + dc_y1 - (dc_y1 >> 7);   /* DC blocker, ~10 Hz at 8 kHz */
    dc_x1 = x;
    dc_y1 = y;

    int32_t v = y * (1 << AUDIO_GAIN_SHIFT);
    if(v > 8388607)  { v = 8388607; }
    if(v < -8388608) { v = -8388608; }
    tx[i] = (uint32_t)v & 0x00FFFFFF;
  }
}

static void
i2s_handler(nrfx_i2s_buffers_t const *p_released, uint32_t status)
{
  i2s_cb_count++;

  if(status & NRFX_I2S_STATUS_NEXT_BUFFERS_NEEDED) {
    nrfx_i2s_buffers_t next = { 0 };
    next.p_rx_buffer = buf_rx[next_idx];
    next.p_tx_buffer = buf_tx[next_idx];
    next.buffer_size = AUDIO_SAMPLES_PER_BLOCK;
    nrfx_i2s_next_buffers_set(&i2s_inst, &next);
    next_idx ^= 1;

    if(p_released != NULL && p_released->p_rx_buffer != NULL
       && p_released->p_tx_buffer != NULL) {
      const uint32_t *rx = (const uint32_t *)p_released->p_rx_buffer;

      /* two consecutive mic samples, printed from main */
      dbg_rx[0] = ((int32_t)(rx[0] << 8)) >> 8;
      dbg_rx[1] = ((int32_t)(rx[1] << 8)) >> 8;

      audio_loopback_block(rx, (uint32_t *)p_released->p_tx_buffer);
    }
  }
}

int
audio_init(void)
{
  i2s_cfg = (nrfx_i2s_config_t)NRFX_I2S_DEFAULT_CONFIG(
      I2S_SCK_PIN, I2S_LRCK_PIN, NRF_I2S_PIN_NOT_CONNECTED,
      I2S_SDOUT_PIN, I2S_SDIN_PIN);
  i2s_cfg.channels = NRF_I2S_CHANNELS_LEFT;
  i2s_cfg.mck_setup = NRF_I2S_MCK_32MDIV21;     /* MCK = ~1.524 MHz */
  i2s_cfg.sample_width = NRF_I2S_SWIDTH_24BIT;
  i2s_cfg.ratio = NRF_I2S_RATIO_192X;           /* LRCK ~7.94 kHz, SCK ~381 kHz */

  nrfx_err_t err = nrfx_i2s_init(&i2s_inst, &i2s_cfg, i2s_handler);
  printf("audio_init: i2s_init result 0x%lx\n", (unsigned long)err);
  return (err == NRFX_SUCCESS) ? 0 : -1;
}

int
audio_start(void)
{
  nrfx_i2s_buffers_t initial = { 0 };
  initial.p_rx_buffer = buf_rx[0];
  initial.p_tx_buffer = buf_tx[0];
  initial.buffer_size = AUDIO_SAMPLES_PER_BLOCK;
  next_idx = 1;

  nrfx_err_t err = nrfx_i2s_start(&i2s_inst, &initial, 0);
  printf("audio_start: i2s_start result 0x%lx\n", (unsigned long)err);
  return (err == NRFX_SUCCESS) ? 0 : -1;
}