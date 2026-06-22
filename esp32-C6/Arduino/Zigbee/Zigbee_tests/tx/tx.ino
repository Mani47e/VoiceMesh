#ifndef ZIGBEE_MODE_ZCZR
#error "Set Tools > Zigbee Mode > Zigbee ZCZR (Coordinator/Router)"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeCustomCluster.h"

// Custom cluster and attribute IDs (must match on both sides)
#define MY_CLUSTER_ID   0xFF00
#define MY_ATTR_STRING  0x0001

ZigbeeCustomCluster textCluster(MY_CLUSTER_ID, ZIGBEE_COORDINATOR);
ZigbeeEP myEP;

void setup() {
  Serial.begin(115200);

  // Create a string attribute
  textCluster.addAttribute(MY_ATTR_STRING, ZB_ATTR_TYPE_CHAR_STRING, 64);

  myEP.setVersion(1);
  myEP.addCluster(textCluster);

  Zigbee.addEndpoint(myEP);
  Zigbee.begin();

  Serial.println("Coordinator started. Type text to send:");
}

void loop() {
  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) {
      textCluster.setAttribute(MY_ATTR_STRING, (void*)msg.c_str(), msg.length() + 1);
      Serial.println("Sent: " + msg);
    }
  }
}