#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>
#include <stddef.h>

#define MIC_GAIN 8

/* Converts 24-bit aligned PCM samples to 16-bit clamped samples with gain */
void codec_process_mic_block(const int32_t *in, int16_t *out, size_t samples, int32_t *peak_out);

/* Converts a 16-bit PCM sample to 24-bit aligned output for I2S TX */
static inline int32_t codec_expand_sample(int16_t sample)
{
        return ((int32_t)sample) << 16;
}

#endif /* CODEC_H */