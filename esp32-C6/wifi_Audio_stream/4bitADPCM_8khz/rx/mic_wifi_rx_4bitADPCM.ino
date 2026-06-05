#include <Arduino.h>
#include <driver/i2s_std.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define I2S_BCLK  5
#define I2S_LRC   4
#define I2S_DOUT  7

const char* WIFI_SSID = "Mani";
const char* WIFI_PASS = "eeeeeeee";
const int   UDP_PORT  = 3000;

WiFiUDP udp;
static i2s_chan_handle_t tx_chan;

// ── IMA ADPCM tables (same as sender) ───────────────────────
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

struct AdpcmState {
  int16_t predictor = 0;
  int     step_index = 0;
};

// Decode one 4-bit nibble → 16-bit sample
int16_t adpcm_decode_sample(uint8_t nibble, AdpcmState &state) {
  int step = step_table[state.step_index];
  int delta = step >> 3;
  if (nibble & 1) delta += step >> 2;
  if (nibble & 2) delta += step >> 1;
  if (nibble & 4) delta += step;
  if (nibble & 8) delta = -delta;

  state.predictor = constrain(state.predictor + delta, -32768, 32767);
  state.step_index = constrain(state.step_index + index_table[nibble & 0x0F], 0, 88);

  return state.predictor;
}

// Decode packed ADPCM bytes → 16-bit PCM samples
// Each byte contains 2 nibbles → 2 samples
int adpcm_decode(uint8_t* input, int num_bytes, int16_t* output, AdpcmState &state) {
  int out_idx = 0;
  for (int i = 0; i < num_bytes; i++) {
    output[out_idx++] = adpcm_decode_sample(input[i] & 0x0F, state);       // low nibble
    output[out_idx++] = adpcm_decode_sample((input[i] >> 4) & 0x0F, state); // high nibble
  }
  return out_idx;
}

// ── Ring buffer ──────────────────────────────────────────────
#define RING_SIZE 8000
static int16_t ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

inline int ring_available() {
  return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

// Global buffers
static uint8_t  udp_buf[256];
static int16_t  decoded[512];   // 256 bytes → 512 samples
static int16_t  play_buf[256];
static AdpcmState dec_state;

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nReceiver IP: " + WiFi.localIP().toString());
  Serial.println(">>> Put this IP in RECEIVER_IP on sender <<<");

  udp.begin(UDP_PORT);

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, &tx_chan, NULL);

  i2s_std_clk_config_t  clk  = I2S_STD_CLK_DEFAULT_CONFIG(8000);
  i2s_std_slot_config_t slot = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(
    I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO
  );
  i2s_std_gpio_config_t gpio = {
    .mclk = I2S_GPIO_UNUSED,
    .bclk = (gpio_num_t)I2S_BCLK,
    .ws   = (gpio_num_t)I2S_LRC,
    .dout = (gpio_num_t)I2S_DOUT,
    .din  = I2S_GPIO_UNUSED,
    .invert_flags = { false, false, false },
  };
  i2s_std_config_t std_cfg = {
    .clk_cfg  = clk,
    .slot_cfg = slot,
    .gpio_cfg = gpio,
  };

  i2s_channel_init_std_mode(tx_chan, &std_cfg);
  i2s_channel_enable(tx_chan);
  Serial.println("Ready — waiting for ADPCM audio...");
}

void loop() {
  // ── Receive UDP → decode ADPCM → ring buffer ────────────
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(udp_buf, sizeof(udp_buf));

    // Decode ADPCM: 128 bytes → 256 samples
    int samples = adpcm_decode(udp_buf, len, decoded, dec_state);

    // Write decoded samples into ring
    for (int i = 0; i < samples; i++) {
      int next = (ring_head + 1) % RING_SIZE;
      if (next != ring_tail) {
        ring[ring_head] = decoded[i];
        ring_head = next;
      }
    }
  }

  // ── Ring buffer → I2S ───────────────────────────────────
  if (ring_available() >= 256) {
    int to_play = min(256, ring_available());
    for (int i = 0; i < to_play; i++) {
      play_buf[i] = ring[ring_tail];
      ring_tail = (ring_tail + 1) % RING_SIZE;
    }
    size_t written;
    i2s_channel_write(tx_chan, play_buf, to_play * sizeof(int16_t), &written, 0);
  }
}