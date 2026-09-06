#include "codec.h"

void codec_process_mic_block(const int32_t *in, int16_t *out, size_t samples, int32_t *peak_out)
{
        int32_t peak = 0;

        for (size_t i = 0; i < samples; i++) {
                int32_t shifted = in[i] >> 16;
                int32_t boosted = shifted * MIC_GAIN;

                if (boosted > 32767) {
                        boosted = 32767;
                } else if (boosted < -32768) {
                        boosted = -32768;
                }

                out[i] = (int16_t)boosted;

                int32_t mag = boosted < 0 ? -boosted : boosted;
                if (mag > peak) {
                        peak = mag;
                }
        }

        if (peak_out) {
                *peak_out = peak;
        }
}