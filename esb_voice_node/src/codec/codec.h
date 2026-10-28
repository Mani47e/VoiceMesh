#ifndef CODEC_H
#define CODEC_H

#include <stdint.h>
#include <stddef.h>

#define MIC_GAIN 8

void codec_process_mic_block(const int32_t *in, int16_t *out, size_t samples, int32_t *peak_out);

static inline int32_t codec_expand_sample(int16_t sample)
{
        return ((int32_t)sample) << 16;
}

#endif /* CODEC_H */