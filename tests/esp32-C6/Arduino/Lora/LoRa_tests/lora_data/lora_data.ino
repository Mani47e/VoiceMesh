#include <HardwareSerial.h>
#include <driver/i2s.h>

HardwareSerial LoRaSerial(1);
#define LORA_RX 4
#define LORA_TX 5
#define DEST_ADDR 2

// INMP441 pins
#define I2S_WS  6
#define I2S_SCK 7
#define I2S_SD  10

#define SAMPLE_RATE    8000
#define RECORD_SECONDS 2
#define RAW_SAMPLES    (SAMPLE_RATE * RECORD_SECONDS)

// ── ADPCM tables ──────────────────────────────────
const int8_t indexTable[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};
const int16_t stepTable[89] = {
  7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
  50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,
  253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,
  1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,
  3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,
  10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,
  27086,29794,32767
};

struct AdpcmState { int16_t prevsample; int8_t previndex; };

uint8_t adpcmEncode(int16_t sample, AdpcmState &s) {
  int16_t step = stepTable[s.previndex];
  int32_t diff = sample - s.prevsample;
  uint8_t code = 0;
  if (diff < 0) { code = 8; diff = -diff; }
  if (diff >= step)       { code |= 4; diff -= step; }
  if (diff >= step >> 1)  { code |= 2; diff -= step >> 1; }
  if (diff >= step >> 2)  { code |= 1; }
  int32_t dq = step >> 3;
  if (code & 4) dq += step;
  if (code & 2) dq += step >> 1;
  if (code & 1) dq += step >> 2;
  s.prevsample = (int16_t)constrain(
    (code & 8) ? s.prevsample - dq : s.prevsample + dq, -32768, 32767);
  s.previndex = constrain(s.previndex + indexTable[code & 7], 0, 88);
  return code;
}

void setupI2S() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
  };
  i2s_pin_config_t pins = {
    .bck_io_num = I2S_SCK,
    .ws_io_num  = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_SD
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
}

void sendAT(String cmd) {
  LoRaSerial.println(cmd);
  delay(300);
  while (LoRaSerial.available()) Serial.write(LoRaSerial.read());
}

void waitOK() {
  unsigned long t = millis();
  while (millis() - t < 2000) {
    if (LoRaSerial.available()) {
      String r = LoRaSerial.readStringUntil('\n');
      if (r.indexOf("+OK") >= 0) return;
    }
  }
}

void sendPacket(uint8_t seq, bool isLast, uint8_t* data, int len) {
  char buf[3];
  String hex = "";
  sprintf(buf, "%02X", seq);          hex += buf;
  sprintf(buf, "%02X", isLast?1:0);   hex += buf;
  for (int i = 0; i < len; i++) {
    sprintf(buf, "%02X", data[i]);
    hex += buf;
  }
  String cmd = "AT+SEND=" + String(DEST_ADDR) + ","
             + String(hex.length()) + "," + hex;
  LoRaSerial.println(cmd);
  waitOK();
  Serial.printf("Sent pkt #%d (%s)\n", seq, isLast?"LAST":"...");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  LoRaSerial.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);
  delay(1000);
  while (LoRaSerial.available()) LoRaSerial.read();

  sendAT("AT");
  sendAT("AT+ADDRESS=1");
  sendAT("AT+NETWORKID=6");
  sendAT("AT+BAND=865000000");
  sendAT("AT+PARAMETER=9,7,1,12");
  setupI2S();

  Serial.println("=== SENDER READY ===");
  Serial.println("Press ENTER to record and send.");
}

void loop() {
  if (!Serial.available()) return;
  Serial.read();

  // 1. Record
  Serial.println("Recording...");
  int16_t* pcm = (int16_t*)malloc(RAW_SAMPLES * sizeof(int16_t));
  if (!pcm) { Serial.println("malloc failed"); return; }

  size_t bytesRead;
  int32_t raw32;
  for (int i = 0; i < RAW_SAMPLES; i++) {
    i2s_read(I2S_NUM_0, &raw32, sizeof(raw32), &bytesRead, portMAX_DELAY);
    pcm[i] = (int16_t)(raw32 >> 14);
  }
  Serial.println("Done. Compressing...");

  // 2. ADPCM encode
  int adpcmLen = RAW_SAMPLES / 2;
  uint8_t* adpcm = (uint8_t*)malloc(adpcmLen);
  if (!adpcm) { free(pcm); Serial.println("malloc failed"); return; }

  AdpcmState state = {0, 0};
  for (int i = 0; i < RAW_SAMPLES; i += 2) {
    uint8_t lo = adpcmEncode(pcm[i],   state) & 0x0F;
    uint8_t hi = adpcmEncode(pcm[i+1], state) & 0x0F;
    adpcm[i/2] = lo | (hi << 4);
  }
  free(pcm);
  Serial.printf("Compressed: %d bytes. Sending...\n", adpcmLen);

  // 3. Send in 100-byte chunks (hex = 200 chars, within 240 limit)
  uint8_t seq = 0;
  int offset = 0;
  while (offset < adpcmLen) {
    int chunk = min(100, adpcmLen - offset);
    bool last = (offset + chunk >= adpcmLen);
    sendPacket(seq++, last, adpcm + offset, chunk);
    offset += chunk;
    delay(100);
  }
  free(adpcm);
  Serial.println("All sent! Press ENTER to record again.");
}