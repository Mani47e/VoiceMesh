#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_SCK  4
#define I2S_WS   5
#define I2S_SD   6

#define SAMPLE_RATE  8000
#define DMA_BUF_LEN  512

// ---- 2-bit ADPCM tables ----
const int indexTable2bit[4] = { -1, 2, -1, 2 };

const int stepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

struct ADPCMState {
    int16_t predicted = 0;
    int     stepIndex = 0;
};

ADPCMState state;

uint8_t encodeADPCM2(int16_t sample, ADPCMState& state) {
    int step  = stepTable[state.stepIndex];
    int delta = sample - state.predicted;

    uint8_t nibble = 0;

    if (delta < 0) {
        nibble = 2;
        delta  = -delta;
    }

    int diff;
    if (delta > (step >> 1)) {
        nibble |= 1;
        diff    = step + (step >> 2);
    } else {
        diff = (step >> 2);
    }

    if (nibble & 2) diff = -diff;

    state.predicted = (int16_t)constrain(
        (int)state.predicted + diff, -32768, 32767
    );
    state.stepIndex = constrain(
        state.stepIndex + indexTable2bit[nibble], 0, 88
    );

    return nibble & 0x03;
}

void initI2S() {
    i2s_config_t cfg = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = false,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_SCK,
        .ws_io_num    = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_SD
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
    i2s_zero_dma_buffer(I2S_NUM_0);

    // Flush startup garbage
    int32_t dummy[128];
    size_t dumped = 0;
    while (dumped < SAMPLE_RATE) {
        size_t br = 0;
        i2s_read(I2S_NUM_0, dummy, sizeof(dummy), &br, portMAX_DELAY);
        dumped += br / sizeof(int32_t);
    }
}

void setup() {
    Serial.begin(115200);  // 2-bit ADPCM at 8kHz = 2000 bytes/sec, 115200 is fine
    delay(500);

    initI2S();
}

void loop() {
    int32_t raw[128];
    size_t  bytes_read = 0;

    i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
    int count = bytes_read / sizeof(int32_t);

    // Pack 4 samples into each byte
    uint8_t adpcm[32];  // 128 samples / 4 = 32 bytes
    int outCount = 0;

    for (int i = 0; i + 3 < count; i += 4) {
        int16_t s0 = (int16_t)(raw[i]   >> 16);
        int16_t s1 = (int16_t)(raw[i+1] >> 16);
        int16_t s2 = (int16_t)(raw[i+2] >> 16);
        int16_t s3 = (int16_t)(raw[i+3] >> 16);

        uint8_t n0 = encodeADPCM2(s0, state);  // bits [1:0]
        uint8_t n1 = encodeADPCM2(s1, state);  // bits [3:2]
        uint8_t n2 = encodeADPCM2(s2, state);  // bits [5:4]
        uint8_t n3 = encodeADPCM2(s3, state);  // bits [7:6]

        adpcm[outCount++] = n0 | (n1 << 2) | (n2 << 4) | (n3 << 6);
    }

    Serial.write(adpcm, outCount);
}