#include "../mqtt.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../wifi.h"

static bool sConnected = false;

#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
#include <WiFi.h>
#include <PubSubClient.h>
static WiFiClient sWifiClient;
static PubSubClient sMqtt(sWifiClient);
#endif

void mqttInit() {
  sConnected = false;
#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
  if (MQTT_BROKER[0] == '\0') {
    LOG_WARN("MQTT enabled but MQTT_BROKER is empty");
    return;
  }
  sMqtt.setServer(MQTT_BROKER, MQTT_PORT);
  LOG_INFO("MQTT init");
#else
  LOG_DEBUG("MQTT disabled");
#endif
}

void mqttLoop() {
#if MQTT_ENABLED
#if defined(SMARTFARM_TEST_HOOKS)
  sConnected = true;
#else
  if (MQTT_BROKER[0] == '\0' || WiFi.status() != WL_CONNECTED) {
    sConnected = false;
    return;
  }
  if (!sMqtt.connected()) {
    // Tránh thử kết nối lại ở tần số loop() (có thể hàng nghìn lần/giây).
    static unsigned long sLastAttempt = 0;
    const unsigned long now = millis();
    if (now - sLastAttempt < 5000UL) return;
    sLastAttempt = now;
    sConnected = sMqtt.connect("smartfarm");
  } else {
    sMqtt.loop();
    sConnected = true;
  }
#endif
#else
  sConnected = false;
#endif
}

bool mqttPublish(const char* topic, const char* payload) {
  if (!sConnected) return false;
#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
  return sMqtt.publish(topic, payload);
#else
  (void)topic; (void)payload;
  return true;
#endif
}

void mqttSubscribe(const char* topic) {
#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
  if (sConnected) sMqtt.subscribe(topic);
#else
  (void)topic;
#endif
}

bool mqttConnected() { return sConnected; }
