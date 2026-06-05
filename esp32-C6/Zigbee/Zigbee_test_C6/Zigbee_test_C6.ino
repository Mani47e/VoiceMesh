#include "Zigbee.h"

// We abuse the Analog Value cluster to send a float,
// but for text we use ZigbeeAnalog's "description" string attribute.
// For true string transmission we use a workaround:
// encode each char as a float report, OR use custom cluster.
// SIMPLEST approach: send text byte-by-byte as analog value (uint8).

#define ZIGBEE_ENDPOINT 10
#define SENDER_ROLE     ZIGBEE_END_DEVICE

ZigbeeAnalog zbAnalog = ZigbeeAnalog(ZIGBEE_ENDPOINT);

void setup() {
  Serial.begin(115200);

  // Add analog endpoint — we'll use "present_value" to send byte values
  zbAnalog.setAnalogInput(true);   // we are the reporter
  zbAnalog.addAnalogInput();

  Zigbee.addEndpoint(&zbAnalog);

  Serial.println("Starting Zigbee End Device (text sender)...");
  if (!Zigbee.begin(SENDER_ROLE)) {
    Serial.println("Zigbee failed to start!");
    while (1) delay(100);
  }

  Serial.println("Waiting to join coordinator...");
  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nJoined! Ready to send text.");
}

void loop() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    Serial.print("Sending: ");
    Serial.println(msg);

    // Send each character as a separate analog report
    for (int i = 0; i < msg.length(); i++) {
      zbAnalog.setAnalogInputValue((float)msg[i]);
      zbAnalog.reportAnalogInput();
      delay(100);  // small gap between chars
    }
    // Send 0 as end-of-message marker
    zbAnalog.setAnalogInputValue(0.0f);
    zbAnalog.reportAnalogInput();
  }
  delay(10);
}