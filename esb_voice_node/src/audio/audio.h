#ifndef AUDIO_H
#define AUDIO_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stddef.h>

#define SAMPLE_RATE             8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT       4
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK)

int audio_init(void);
int audio_start(void);
void audio_preload_silence(void);

int audio_read(void **buf, size_t *size);
void audio_free_rx(void *buf);

int audio_alloc_tx(void **buf);
int audio_write(void *buf, size_t size);
void audio_free_tx(void *buf);

#endif /* AUDIO_H */