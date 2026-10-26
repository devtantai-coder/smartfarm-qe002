#include "../mqtt.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../wifi.h"

static bool sConnected = true;

#ifndef SMARTFARM_TEST_HOOKS
#include <WiFi.h>
#include <PubSubClient.h>
static WiFiClient sWifiClient;
static PubSubClient sMqtt(sWifiClient);
static char sBroker[64] = "";
#endif

void mqttInit() {
  LOG_INFO("MQTT init");
  sConnected = false;
#ifndef SMARTFARM_TEST_HOOKS
  sMqtt.setServer(sBroker, 1883);
#endif
}

void mqttLoop() {
#ifndef SMARTFARM_TEST_HOOKS
  if (WiFi.status() != WL_CONNECTED) { sConnected = false; return; }
  if (!sMqtt.connected()) {
    sConnected = sMqtt.connect("smartfarm");
  } else {
    sMqtt.loop();
    sConnected = true;
  }
#else
  sConnected = true;
#endif
}

bool mqttPublish(const char* topic, const char* payload) {
  if (!sConnected) return false;
#ifndef SMARTFARM_TEST_HOOKS
  return sMqtt.publish(topic, payload);
#else
  (void)topic; (void)payload;
  return true;
#endif
}

void mqttSubscribe(const char* topic) {
#ifndef SMARTFARM_TEST_HOOKS
  sMqtt.subscribe(topic);
#else
  (void)topic;
#endif
}

bool mqttConnected() { return sConnected; }
