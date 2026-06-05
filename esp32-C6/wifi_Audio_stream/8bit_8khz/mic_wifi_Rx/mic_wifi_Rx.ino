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

// ─── Ring buffer ───────────────────────────────────────────
// Holds 8000 samples = 1 full second of audio at 8kHz
// Head moves forward as UDP writes in
// Tail moves forward as I2S reads out
// They chase each other around the circle forever
#define RING_SIZE 8000
static uint8_t ring[RING_SIZE];   // stores raw 8-bit unsigned samples
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

// How many samples are waiting to be played
inline int ring_available() {
  return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}
// ───────────────────────────────────────────────────────────

// Global buffers
static uint8_t udp_buf[512];
static int16_t play_buf[256];

void setup() {
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nReceiver IP: " + WiFi.localIP().toString());
  Serial.println(">>> Put this IP in RECEIVER_IP on the sender <<<");

  udp.begin(UDP_PORT);

  // I2S output init
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
  Serial.println("I2S ready — waiting for audio...");
}

void loop() {

  // ── STEP 1: UDP → Ring buffer (write side) ──────────────
  // This runs as fast as possible, never blocks
  // Incoming 8-bit samples get dropped into the ring
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(udp_buf, sizeof(udp_buf));
    for (int i = 0; i < len; i++) {
      int next = (ring_head + 1) % RING_SIZE;
      if (next != ring_tail) {        // only write if ring not full
        ring[ring_head] = udp_buf[i];
        ring_head = next;
      }
      // if full, packet is silently dropped — better than stuttering
    }
  }

  // ── STEP 2: Ring buffer → I2S (read side) ───────────────
  // Wait for at least 256 samples before starting playback
  // This is the "jitter buffer" — absorbs WiFi timing variation
  // Too small = glitchy, too large = noticeable delay
  if (ring_available() >= 256) {
    int to_play = min(256, ring_available());

    // Convert 8-bit unsigned → 16-bit signed for I2S
    // This is just reversing what the sender did:
    // sender:   16bit → (s >> 8) + 128  → 8bit unsigned
    // receiver: 8bit  → (val - 128) << 8 → 16bit signed
    for (int i = 0; i < to_play; i++) {
      play_buf[i] = ((int16_t)ring[ring_tail] - 128) << 8;
      ring_tail = (ring_tail + 1) % RING_SIZE;
    }

    size_t written;
    // Timeout 0 = non-blocking
    // If I2S is busy this chunk is dropped rather than blocking
    // which would cause UDP packets to pile up and stutter
    i2s_channel_write(tx_chan, play_buf, to_play * sizeof(int16_t), &written, 0);
  }
}