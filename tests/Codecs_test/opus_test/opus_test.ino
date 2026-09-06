#include <Arduino.h>
#include <driver/i2s.h>
#include "opus.h"

#define I2S_SCK  19
#define I2S_WS   20
#define I2S_SD   18

#define SAMPLE_RATE     8000
#define FRAME_SIZE      160
#define MAX_PACKET_SIZE 256

OpusEncoder *encoder;

// Move ALL buffers to global — off the stack
int32_t raw[128];
int16_t pcmBuf[FRAME_SIZE];
uint8_t opusBuf[MAX_PACKET_SIZE];
int pcmIndex = 0;

void setup() {
  Serial.begin(921600);
  delay(500);

  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

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

  int err;
  encoder = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
  if (err != OPUS_OK || encoder == NULL) {
    Serial.printf("Opus failed: %d\n", err);
    while(1) delay(100);
  }
  opus_encoder_ctl(encoder, OPUS_SET_BITRATE(8000));
  opus_encoder_ctl(encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_NARROWBAND));
  opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(3));

  Serial.println("Opus ready, streaming...");

  // Increase loopTask stack to 16KB
// Add at very top of setup(), before i2s/opus init
vTaskDelete(NULL); // won't work this way — see below
}

void loop() {
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);
  int count = bytes_read / sizeof(int32_t);

  for (int i = 0; i < count; i++) {
    pcmBuf[pcmIndex++] = (int16_t)(raw[i] >> 16);

    if (pcmIndex >= FRAME_SIZE) {
      int encodedBytes = opus_encode(encoder, pcmBuf, FRAME_SIZE,
                                     opusBuf, MAX_PACKET_SIZE);
      if (encodedBytes > 0) {
        uint8_t header[2] = {
          (uint8_t)(encodedBytes >> 8),
          (uint8_t)(encodedBytes & 0xFF)
        };
        Serial.write(header, 2);
        Serial.write(opusBuf, encodedBytes);
      }
      pcmIndex = 0;
    }
  }
}