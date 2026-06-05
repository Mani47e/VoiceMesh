#include <Arduino.h>
#include <driver/i2s.h>
extern "C" {
  #include "codec2.h"
}

#define I2S_SCK  19
#define I2S_WS   20
#define I2S_SD   18
#define SAMPLE_RATE 8000

// Try modes in this order (best→smallest):
// CODEC2_MODE_3200 → 8 bytes/frame, best quality
// CODEC2_MODE_2400 → 6 bytes/frame
// CODEC2_MODE_1600 → 8 bytes/frame, 2x compression
// CODEC2_MODE_700C → 4 bytes/frame, most compressed
#define C2_MODE CODEC2_MODE_3200

struct CODEC2 *c2;
int samplesPerFrame;
int bytesPerFrame;

// Global buffers — avoid stack overflow
int32_t  raw[128];
int16_t *pcmBuf;
uint8_t *c2Buf;
int pcmIndex = 0;

void setup() {
  Serial.begin(921600);
  delay(500);
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

  // I2S
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
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = -1,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);

  // Codec2
  c2              = codec2_create(C2_MODE);
  samplesPerFrame = codec2_samples_per_frame(c2);
  bytesPerFrame   = codec2_bytes_per_frame(c2);
  pcmBuf          = (int16_t*)malloc(samplesPerFrame * sizeof(int16_t));
  c2Buf           = (uint8_t*)malloc(bytesPerFrame);

  Serial.printf("Codec2 ready: %d samples/frame, %d bytes/frame\n",
                samplesPerFrame, bytesPerFrame);
}

void loop() {
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
  int count = bytes_read / sizeof(int32_t);

  for (int i = 0; i < count; i++) {
    pcmBuf[pcmIndex++] = (int16_t)(raw[i] >> 16);

    if (pcmIndex >= samplesPerFrame) {
      codec2_encode(c2, c2Buf, pcmBuf);

      // 1-byte length header + codec2 frame
      Serial.write((uint8_t)bytesPerFrame);
      Serial.write(c2Buf, bytesPerFrame);

      pcmIndex = 0;
    }
  }
}