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

// ── IMA ADPCM tables ────────────────────────────────────────
const int step_table[89] = {
  7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
  50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,
  253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,
  1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
  3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
  10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};

const int index_table[16] = {
  -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};

// ADPCM encoder state
struct AdpcmState {
  int16_t predictor = 0;
  int     step_index = 0;
};

// Encode one 16-bit sample → 4-bit nibble
uint8_t adpcm_encode_sample(int16_t sample, AdpcmState &state) {
  int step = step_table[state.step_index];
  int diff = sample - state.predictor;
  uint8_t nibble = 0;

  if (diff < 0) { nibble = 8; diff = -diff; }
  if (diff >= step)        { nibble |= 4; diff -= step; }
  if (diff >= step / 2)    { nibble |= 2; diff -= step / 2; }
  if (diff >= step / 4)    { nibble |= 1; }

  // Update predictor
  step = step_table[state.step_index];
  int delta = step >> 3;
  if (nibble & 1) delta += step >> 2;
  if (nibble & 2) delta += step >> 1;
  if (nibble & 4) delta += step;
  if (nibble & 8) delta = -delta;

  state.predictor = constrain(state.predictor + delta, -32768, 32767);
  state.step_index = constrain(state.step_index + index_table[nibble & 0x0F], 0, 88);

  return nibble & 0x0F;
}

// Encode block of 16-bit samples → packed 4-bit ADPCM bytes
// Returns number of output bytes
int adpcm_encode(int16_t* input, int num_samples, uint8_t* output, AdpcmState &state) {
  int out_idx = 0;
  for (int i = 0; i < num_samples; i += 2) {
    uint8_t lo = adpcm_encode_sample(input[i],     state);
    uint8_t hi = adpcm_encode_sample(input[i + 1], state);
    output[out_idx++] = (hi << 4) | lo;  // pack two nibbles per byte
  }
  return out_idx;
}

// Global buffers
int32_t  raw[256];
int16_t  pcm16[256];
uint8_t  adpcm_out[128];  // half size — 256 samples → 128 bytes
AdpcmState enc_state;

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
  Serial.println("Streaming ADPCM...");
}

void loop() {
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw, sizeof(raw), &bytes_read, portMAX_DELAY);

  int count = bytes_read / sizeof(int32_t);

  // 32-bit → 16-bit
  for (int i = 0; i < count; i++) {
    pcm16[i] = (int16_t)(raw[i] >> 16);
  }

  // Encode to ADPCM — 256 samples → 128 bytes
  int out_bytes = adpcm_encode(pcm16, count, adpcm_out, enc_state);

  udp.beginPacket(RECEIVER_IP, UDP_PORT);
  udp.write(adpcm_out, out_bytes);
  udp.endPacket();
}