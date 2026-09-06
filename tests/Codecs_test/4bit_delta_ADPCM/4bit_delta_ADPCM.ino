#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_SCK  19
#define I2S_WS   20
#define I2S_SD   18

#define SAMPLE_RATE 8000

int16_t predictor = 128;   // Start near mid-scale

void setup() {
    Serial.begin(921600);
    delay(500);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SCK,
        .ws_io_num = I2S_WS,
        .data_out_num = -1,
        .data_in_num = I2S_SD
    };

    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void loop() {

    int32_t raw[128];
    size_t bytes_read = 0;

    i2s_read(I2S_NUM_0,
             raw,
             sizeof(raw),
             &bytes_read,
             portMAX_DELAY);

    int count = bytes_read / sizeof(int32_t);

    // 128 samples -> 64 compressed bytes
    uint8_t packed[64];

    for(int i = 0; i < count; i += 2) {

        uint8_t codes[2];

        for(int k = 0; k < 2; k++) {

            int16_t s = (int16_t)(raw[i+k] >> 16);

            uint8_t pcm =
                (uint8_t)((s >> 8) + 128);

            int16_t diff =
                (int16_t)pcm - predictor;

            if(diff > 7)  diff = 7;
            if(diff < -8) diff = -8;

            predictor += diff;

            codes[k] = diff & 0x0F;
        }

        packed[i/2] =
            (codes[0] << 4) |
            codes[1];
    }

    Serial.write(packed, count/2);
}