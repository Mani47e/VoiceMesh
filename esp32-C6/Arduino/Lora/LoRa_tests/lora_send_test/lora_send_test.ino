#define LORA_TX 4
#define LORA_RX 5

HardwareSerial LoRa(1);

void sendAT(String cmd) {
  LoRa.println(cmd);
  delay(200);
  while (LoRa.available()) {
    Serial.println(LoRa.readString());
  }
}

void setup() {
  Serial.begin(115200);
  LoRa.begin(115200, SERIAL_8N1, LORA_RX, LORA_TX);
  delay(1000);

  sendAT("AT");
  sendAT("AT+ADDRESS=1");
  sendAT("AT+NETWORKID=6");
  sendAT("AT+BAND=915000000");
  sendAT("AT+PARAMETER=9,7,1,12");

  Serial.println("Sender ready!");
}

void loop() {
  String msg = "Hello Mani";
  String cmd = "AT+SEND=2," + String(msg.length()) + "," + msg;
  
  LoRa.println(cmd);
  Serial.println("Sent: " + msg);
  delay(200);

  // Print any module response (+OK or +ERR)
  while (LoRa.available()) {
    Serial.println(LoRa.readString());
  }

  delay(5000); // Send every 5 seconds
}