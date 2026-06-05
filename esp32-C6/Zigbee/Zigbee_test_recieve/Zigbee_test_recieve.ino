#include "Zigbee.h"

#define ZIGBEE_ENDPOINT 10
#define RECEIVER_ROLE   ZIGBEE_COORDINATOR

ZigbeeAnalog zbAnalog = ZigbeeAnalog(ZIGBEE_ENDPOINT);

String receivedText = "";

// Callback fires whenever a report arrives from any bound device
void onAnalogReceive(float value, uint8_t endpoint, uint16_t address) {
  if ((int)value == 0) {
    // End of message
    if (receivedText.length() > 0) {
      Serial.print("Received text: ");
      Serial.println(receivedText);
      receivedText = "";
    }
  } else {
    receivedText += (char)(int)value;
  }
}

void setup() {
  Serial.begin(115200);

  zbAnalog.onAnalogInputChange(onAnalogReceive);
  Zigbee.addEndpoint(&zbAnalog);

  Serial.println("Starting Zigbee Coordinator (text receiver)...");
  if (!Zigbee.begin(RECEIVER_ROLE)) {
    Serial.println("Zigbee failed to start!");
    while (1) delay(100);
  }
  Serial.println("Coordinator started. Waiting for devices...");
}

void loop() {
  delay(100);
}