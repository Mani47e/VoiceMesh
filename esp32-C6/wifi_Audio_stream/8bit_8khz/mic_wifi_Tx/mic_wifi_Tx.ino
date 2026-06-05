#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define I2S_SCK     19
#define I2S_WS      20
#define I2S_SD      18
#define SAMPLE_RATE 8000

const char* WIFI_SSID   = "Mani";
const char* WIFI_PASS   = "eeeeeeee";
const char* RECEIVER_IP = "192.168.102.135";
const int   UDP_PORT    = 3000;

WiFiUDP udp;

// Global to avoid stack issues
int32_t raw[256];
uint8_t pcm[256];

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nSender IP: " + WiFi.localIP().toString());

  udp.begin(UDP_PORT);

  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);

  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_SCK,
    .ws_io_num    = I2S_WS,
    .data_out_num = -1,
    .data_in_num  = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("Streaming...");
}

void loop() {
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);

  int count = bytes_read / sizeof(int32_t);
  for (int i = 0; i < count; i++) {
    int16_t s = (int16_t)(raw[i] >> 16);   // 32bit → 16bit
    pcm[i] = (uint8_t)((s >> 8) + 128);    // 16bit → 8bit unsigned
  }

  udp.beginPacket(RECEIVER_IP, UDP_PORT);
  udp.write(pcm, count);   // only 1 byte per sample — small packets
  udp.endPacket();
}