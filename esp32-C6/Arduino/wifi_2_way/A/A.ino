#include <Arduino.h>
#include <driver/i2s.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ─── CONFIG — only thing you need to change ───────────────
const char* WIFI_SSID = "Pritam";
const char* WIFI_PASS = "pritam10";
const char* PEER_IP_A = "10.97.50.135";   // first device IP
const char* PEER_IP_B = "10.97.50.239";   // second device IP
const int   UDP_PORT  = 3000;

#define I2S_BCLK    5
#define I2S_WS      4
#define I2S_MIC_SD  6
#define I2S_SPK_DIN 7
#define PTT_PIN     15
#define SAMPLE_RATE 8000
// ──────────────────────────────────────────────────────────

WiFiUDP udp;
const char* peer_ip = nullptr;

// I2S buffers
static int32_t mic_raw[256];
static int16_t spk_buf[256];

// UDP receive buffer
static uint8_t udp_buf[512];

// ── Ring buffer (1 sec of audio) ──────────────────────────
// Absorbs WiFi jitter — UDP fills it, speaker drains it
#define RING_SIZE 8000
static uint8_t  ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

inline int ring_available() {
  return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

// ── Debug counters ────────────────────────────────────────
static uint32_t dbg_udp_sent    = 0;
static uint32_t dbg_udp_recv    = 0;
static uint32_t dbg_ring_drops  = 0;
static uint32_t dbg_play_chunks = 0;
static int32_t  dbg_mic_peak    = 0;
static uint32_t dbg_last        = 0;

void printDebug() {
  if (millis() - dbg_last < 2000) return;
  dbg_last = millis();
  Serial.println("──────────────────────────────────────");
  Serial.printf("[PTT]   %s\n", digitalRead(PTT_PIN) == LOW ? "TALKING" : "listening");
  Serial.printf("[MIC]   peak=%d\n", dbg_mic_peak);
  Serial.printf("[UDP]   sent=%u  recv=%u\n", dbg_udp_sent, dbg_udp_recv);
  Serial.printf("[RING]  avail=%d  drops=%u\n", ring_available(), dbg_ring_drops);
  Serial.printf("[PLAY]  chunks=%u\n", dbg_play_chunks);
  Serial.println("──────────────────────────────────────");
  dbg_udp_sent = dbg_udp_recv = dbg_ring_drops = dbg_play_chunks = 0;
  dbg_mic_peak = 0;
}

// ─────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WALKIE TALKIE BOOT ===");

  pinMode(PTT_PIN, INPUT_PULLUP);

  // ── WiFi ─────────────────────────────────────────────────
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi connecting");
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (millis() - t > 15000) {
      Serial.println("\n[ERROR] WiFi timeout!"); break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    String myIP = WiFi.localIP().toString();
    Serial.println("\n[WiFi] IP: " + myIP);

    if (myIP == PEER_IP_A) {
      peer_ip = PEER_IP_B;
      Serial.println("[WiFi] I am A → peer is B (" + String(PEER_IP_B) + ")");
    } else if (myIP == PEER_IP_B) {
      peer_ip = PEER_IP_A;
      Serial.println("[WiFi] I am B → peer is A (" + String(PEER_IP_A) + ")");
    } else {
      Serial.println("[ERROR] IP not in config! Add this to PEER_IP_A or B: " + myIP);
      peer_ip = PEER_IP_A;
    }
  }

  udp.begin(UDP_PORT);
  Serial.printf("[UDP] Listening on port %d\n", UDP_PORT);

  // ── I2S — TX + RX, shared BCLK/WS ───────────────────────
  i2s_config_t cfg = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 256,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,   // auto-silence when no write pending
  };
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  Serial.printf("[I2S] driver_install: %s\n", esp_err_to_name(err));

  i2s_pin_config_t pins = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_WS,
    .data_out_num = I2S_SPK_DIN,
    .data_in_num  = I2S_MIC_SD,
  };
  err = i2s_set_pin(I2S_NUM_0, &pins);
  Serial.printf("[I2S] set_pin:        %s\n", esp_err_to_name(err));

  i2s_zero_dma_buffer(I2S_NUM_0);
  Serial.println("\nReady. Hold GPIO15 to talk.\n");
}

// ─────────────────────────────────────────────────────────

void loop() {
  bool talking = (digitalRead(PTT_PIN) == LOW);

  // ── STEP 1: Read mic (always — keeps RX DMA clock alive) ─
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, mic_raw, sizeof(mic_raw), &bytes_read, portMAX_DELAY);
  int sample_count = bytes_read / sizeof(int32_t);

  // Track peak for debug
  for (int i = 0; i < sample_count; i++) {
    int32_t v = abs(mic_raw[i]);
    if (v > dbg_mic_peak) dbg_mic_peak = v;
  }

  // ── STEP 2: If PTT pressed → compress and send via UDP ───
  // 32-bit raw → 8-bit unsigned PCM to keep packets tiny
  // 256 samples = 256 bytes per packet at 8kHz = very low bandwidth
  if (talking && sample_count > 0) {
    static uint8_t mic_pcm[256];
    for (int i = 0; i < sample_count; i++) {
      int16_t s    = (int16_t)(mic_raw[i] >> 16);  // 32→16 bit
      mic_pcm[i]   = (uint8_t)((s >> 8) + 128);    // 16→8 bit unsigned
    }
    udp.beginPacket(peer_ip, UDP_PORT);
    udp.write(mic_pcm, sample_count);
    if (udp.endPacket() == 1) dbg_udp_sent++;
    else Serial.println("[UDP] send FAILED");
  }

  // ── STEP 3: Drain any incoming UDP into ring buffer ───────
  // Non-blocking — if nothing arrived just moves on
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(udp_buf, sizeof(udp_buf));
    dbg_udp_recv++;
    for (int i = 0; i < len; i++) {
      int next = (ring_head + 1) % RING_SIZE;
      if (next != ring_tail) {
        ring[ring_head] = udp_buf[i];
        ring_head = next;
      } else {
        dbg_ring_drops++;  // ring full — peer sending faster than we play
      }
    }
  }

  // ── STEP 4: Play from ring buffer → speaker ───────────────
  // Wait for 256 samples minimum before playing
  // This is the jitter buffer — too small = glitchy, too large = laggy
  int avail = ring_available();
  if (avail >= 256) {
    int to_play = min(256, avail);
    for (int i = 0; i < to_play; i++) {
      // 8-bit unsigned → 16-bit signed (reverse of sender encoding)
      spk_buf[i] = ((int16_t)ring[ring_tail] - 128) << 8;
      ring_tail  = (ring_tail + 1) % RING_SIZE;
    }
    size_t written = 0;
    i2s_write(I2S_NUM_0, spk_buf, to_play * sizeof(int16_t),
              &written, portMAX_DELAY);
    dbg_play_chunks++;
  } else {
    // Ring empty — write silence to keep TX DMA ticking
    memset(spk_buf, 0, sizeof(spk_buf));
    size_t written = 0;
    i2s_write(I2S_NUM_0, spk_buf, sizeof(spk_buf), &written, 0);
  }

  printDebug();
}