#ifndef ZIGBEE_MODE_ED
#error "Set Tools > Zigbee Mode > Zigbee ED (End Device)"
#endif

#include "ZigbeeCore.h"
#include "ep/ZigbeeCustomCluster.h"

#define MY_CLUSTER_ID   0xFF00
#define MY_ATTR_STRING  0x0001

ZigbeeCustomCluster textCluster(MY_CLUSTER_ID, ZIGBEE_END_DEVICE);
ZigbeeEP myEP;

void onStringReceive(uint16_t attrId, void* data, uint16_t len) {
  if (attrId == MY_ATTR_STRING) {
    String received = String((char*)data);
    Serial.println("Received: " + received);
  }
}

void setup() {
  Serial.begin(115200);

  textCluster.addAttribute(MY_ATTR_STRING, ZB_ATTR_TYPE_CHAR_STRING, 64);
  textCluster.onAttributeUpdated(onStringReceive);

  myEP.setVersion(1);
  myEP.addCluster(textCluster);

  Zigbee.addEndpoint(myEP);
  Zigbee.begin();

  Serial.println("End device started, looking for coordinator...");
}

void loop() {
  if (Zigbee.connected()) {
    // Nothing needed — callback handles incoming data
  }
}