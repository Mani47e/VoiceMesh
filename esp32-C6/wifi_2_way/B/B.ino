#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ─── CONFIG ───────────────────────────────────────────────
const char* WIFI_SSID = "Pritam";
const char* WIFI_PASS = "pritam10";
const char* PEER_IP_A = "10.97.50.135";
const char* PEER_IP_B = "10.97.50.200";
const int   UDP_PORT  = 3000;

#define I2S_BCLK    5
#define I2S_WS      4
#define I2S_MIC_SD  6
#define I2S_SPK_DIN 7
#define PTT_PIN     15
#define SAMPLE_RATE 8000

#define MIC_GAIN 8
// ──────────────────────────────────────────────────────────

WiFiUDP udp;
const char* peer_ip = nullptr;

static int32_t mic_raw[256];
static int32_t spk_buf[256];   // 32-bit — matches DMA expectation
static uint8_t udp_buf[512];
static uint8_t mic_pcm[256];

#define RING_SIZE 8000
static uint8_t  ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

inline int ring_available() {
  return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  pinMode(PTT_PIN, INPUT_PULLUP);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (millis() - t > 15000) { Serial.println("\nWiFi timeout!"); break; }
  }

  String myIP = WiFi.localIP().toString();
  Serial.println("\nIP: " + myIP);

  if (myIP == PEER_IP_A)      { peer_ip = PEER_IP_B; Serial.println("I am A → B"); }
  else if (myIP == PEER_IP_B) { peer_ip = PEER_IP_A; Serial.println("I am B → A"); }
  else { Serial.println("IP not in config! Add: " + myIP); peer_ip = PEER_IP_A; }

  udp.begin(UDP_PORT);

  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,  // 32-bit for both RX and TX
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);

  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_WS,
    .data_out_num = I2S_SPK_DIN,
    .data_in_num  = I2S_MIC_SD,
  };
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);

  Serial.println("Ready. Hold GPIO15 to talk.");
}

void loop() {
  bool talking = (digitalRead(PTT_PIN) == LOW);

  // ── Read mic ──────────────────────────────────────────────
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, mic_raw, sizeof(mic_raw), &bytes_read, portMAX_DELAY);
  int sample_count = bytes_read / sizeof(int32_t);

  // ── Send via UDP if PTT pressed ───────────────────────────
 if (talking && sample_count > 0) {
    for (int i = 0; i < sample_count; i++) {
      int16_t s = (int16_t)(mic_raw[i] >> 16);

      // Apply gain then clamp to int16 range so it doesn't wrap/distort
      int32_t boosted = (int32_t)s * MIC_GAIN;
      if (boosted >  32767) boosted =  32767;
      if (boosted < -32768) boosted = -32768;

      mic_pcm[i] = (uint8_t)((boosted >> 8) + 128);
    }
    udp.beginPacket(peer_ip, UDP_PORT);
    udp.write(mic_pcm, sample_count);
    udp.endPacket();
  }

  // ── UDP → ring buffer ─────────────────────────────────────
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(udp_buf, sizeof(udp_buf));
    for (int i = 0; i < len; i++) {
      int next = (ring_head + 1) % RING_SIZE;
      if (next != ring_tail) {
        ring[ring_head] = udp_buf[i];
        ring_head = next;
      }
    }
  }

  // ── Ring → speaker ────────────────────────────────────────
  int avail = ring_available();
  if (avail >= 256) {
    int to_play = min(256, avail);
    for (int i = 0; i < to_play; i++) {
      // 8-bit unsigned → 32-bit signed, placed in upper bits
      // Lower 16 bits left as zero — DMA sees correct amplitude
      spk_buf[i] = ((int32_t)((int16_t)(ring[ring_tail] - 128) << 8)) << 16;
      ring_tail  = (ring_tail + 1) % RING_SIZE;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, spk_buf, to_play * sizeof(int32_t),
              &written, portMAX_DELAY);
  } else {
    memset(spk_buf, 0, sizeof(spk_buf));
    size_t written = 0;
    i2s_write(I2S_NUM_0, spk_buf, sizeof(spk_buf), &written, 0);
  }
}