#ifndef AUDIO_H_
#define AUDIO_H_

#include <stdint.h>

#define I2S_SCK_PIN   2
#define I2S_LRCK_PIN  29
#define I2S_SDIN_PIN  31
#define I2S_SDOUT_PIN 17

#define AUDIO_SAMPLE_RATE       7937    /* MCK 1.524 MHz / RATIO_192X */
#define AUDIO_SAMPLES_PER_BLOCK 128     /* 24-bit mono samples per block (~16 ms) */


#define AUDIO_GAIN_SHIFT 1

extern volatile uint32_t i2s_cb_count;
extern volatile int32_t  dbg_rx[2];

int audio_init(void);
int audio_start(void);

#endif /* AUDIO_H_ */