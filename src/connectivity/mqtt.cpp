#include "mqtt.h"
#include "../core/globals.h"
#include "../core/logger.h"
#include "../core/config.h"
#include "wifi.h"

static bool sConnected = false;
static char sBroker[64] = "";
static uint16_t sPort = 1883;
static unsigned long sLastConnectAttempt = 0;
static unsigned long sBackoffMs = 1000;

#ifndef SMARTFARM_TEST_HOOKS
#include <WiFi.h>
#include <PubSubClient.h>
static WiFiClient sWifiClient;
static PubSubClient sMqtt(sWifiClient);
#endif

void mqttSetBroker(const char* host, uint16_t port) {
  if (!host) return;
  strncpy(sBroker, host, sizeof(sBroker) - 1);
  sBroker[sizeof(sBroker) - 1] = '\0';
  sPort = port ? port : 1883;
#ifndef SMARTFARM_TEST_HOOKS
  sMqtt.setServer(sBroker, sPort);
#endif
  sConnected = false;
  sBackoffMs = 1000;
  LOG_INFO("MQTT broker set");
}

const char* mqttBrokerHost() { return sBroker; }
uint16_t mqttBrokerPort() { return sPort; }

void mqttInit() {
  LOG_INFO("MQTT init");
  sConnected = false;
  sBackoffMs = 1000;
#ifndef SMARTFARM_TEST_HOOKS
  if (sBroker[0] != '\0') sMqtt.setServer(sBroker, sPort);
#endif
}

void mqttLoop() {
#ifndef SMARTFARM_TEST_HOOKS
  if (sBroker[0] == '\0') {
    sConnected = false;
    return;
  }
  if (WiFi.status() != WL_CONNECTED) { sConnected = false; return; }
  unsigned long now = millis();
  if (!sMqtt.connected()) {
    if (now - sLastConnectAttempt >= sBackoffMs) {
      sLastConnectAttempt = now;
      if (sMqtt.connect("smartfarm")) {
        sConnected = true;
        sBackoffMs = 1000;
        LOG_INFO("MQTT connected");
      } else {
        sConnected = false;
        sBackoffMs = (sBackoffMs < 30000) ? (sBackoffMs * 2) : 30000;
      }
    }
  } else {
    sMqtt.loop();
    sConnected = true;
  }
#else
  sConnected = (sBroker[0] != '\0');
#endif
}

bool mqttPublish(const char* topic, const char* payload) {
  if (!sConnected || !topic) return false;
#ifndef SMARTFARM_TEST_HOOKS
  return sMqtt.publish(topic, payload ? payload : "");
#else
  (void)payload;
  return true;
#endif
}

void mqttSubscribe(const char* topic) {
  if (!topic) return;
#ifndef SMARTFARM_TEST_HOOKS
  if (sMqtt.connected()) sMqtt.subscribe(topic);
#endif
}

bool mqttConnected() { return sConnected; }
