#include <HardwareSerial.h>

HardwareSerial LoRaSerial(2);
#define LORA_RX 16
#define LORA_TX 17

#define MAX_BUF 10000
uint8_t adpcmBuf[MAX_BUF];
int     adpcmLen = 0;

void sendAT(String cmd) {
  LoRaSerial.println(cmd);
  delay(300);
  while (LoRaSerial.available()) Serial.write(LoRaSerial.read());
}

uint8_t hexToByte(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  LoRaSerial.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);
  delay(1000);
  while (LoRaSerial.available()) LoRaSerial.read();

  sendAT("AT");
  sendAT("AT+ADDRESS=2");
  sendAT("AT+NETWORKID=6");
  sendAT("AT+BAND=865000000");
  sendAT("AT+PARAMETER=9,7,1,12");

  Serial.println("=== RECEIVER READY ===");
}

void loop() {
  if (!LoRaSerial.available()) return;

  String line = LoRaSerial.readStringUntil('\n');
  line.trim();
  if (!line.startsWith("+RCV=")) return;

  // Parse +RCV=<addr>,<len>,<data>,<rssi>,<snr>
  String payload = line.substring(5);
  int c1 = payload.indexOf(',');
  int c2 = payload.indexOf(',', c1 + 1);
  int c3 = payload.lastIndexOf(',', payload.lastIndexOf(',') - 1);
  String hexData = payload.substring(c2 + 1, c3);

  if (hexData.length() < 4) return;

  uint8_t seq    = (hexToByte(hexData[0]) << 4) | hexToByte(hexData[1]);
  uint8_t isLast = (hexToByte(hexData[2]) << 4) | hexToByte(hexData[3]);
  String chunkHex = hexData.substring(4);

  // Reset buffer on first packet
  if (seq == 0) adpcmLen = 0;

  // Append decoded bytes to buffer
  for (int i = 0; i + 1 < (int)chunkHex.length() && adpcmLen < MAX_BUF; i += 2) {
    adpcmBuf[adpcmLen++] = (hexToByte(chunkHex[i]) << 4)
                          | hexToByte(chunkHex[i + 1]);
  }

  if (isLast) {
    // Send all ADPCM bytes to Python via Serial
    // Format: START marker + length (4 bytes) + raw bytes + END marker
    Serial.write((uint8_t)0xAA);  // START marker
    Serial.write((uint8_t)0xBB);
    Serial.write((uint8_t)(adpcmLen >> 8));   // length high byte
    Serial.write((uint8_t)(adpcmLen & 0xFF)); // length low byte
    Serial.write(adpcmBuf, adpcmLen);         // raw ADPCM bytes
    Serial.write((uint8_t)0xCC);  // END marker
    Serial.write((uint8_t)0xDD);
    adpcmLen = 0;
  }
}