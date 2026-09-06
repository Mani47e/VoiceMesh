#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_SCK  5
#define I2S_WS   4
#define I2S_SD   6

#define SAMPLE_RATE  8000


void setup() {
  Serial.begin(921600);   // High baud — needed for smooth audio streaming
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
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = -1,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);

  i2s_zero_dma_buffer(I2S_NUM_0);
}

void loop() {
  int32_t raw[128];
  size_t bytes_read = 0;

  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);

  int count = bytes_read / sizeof(int32_t);

  // Convert 32-bit I2S frames → 16-bit PCM samples
  uint8_t pcm[128];
  int out = 0;
  for (int i = 0 ; i < count; i++) {
    int16_t s = (int16_t)(raw[i] >> 16);  // take top 16 bits
    pcm[i] = (uint8_t)((s >> 8) + 128); // shift to 8-bit, convert signed→unsigned
  }
  //pcm is the final audio data
  // Send raw 16-bit PCM bytes over Serial
  Serial.write(pcm,count);
}