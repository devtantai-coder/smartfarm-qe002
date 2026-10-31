#include "../mqtt.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../wifi.h"
#include "../actuators.h"
#include "../control.h"

static bool sConnected = false;

#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
#include <WiFi.h>
#include <PubSubClient.h>
#if MQTT_USE_TLS
#include <WiFiClientSecure.h>
static WiFiClientSecure sNetClient;
#else
static WiFiClient sNetClient;
#endif
static PubSubClient sMqtt(sNetClient);
static char sClientId[32] = "smartfarm";
static unsigned long sLastAttempt = 0;
static unsigned long sLastPublish = 0;
// Backoff lũy thừa: broker chết không được làm loop() thử kết nối (blocking)
// mỗi 5 giây mài mãi — mỗi lần connect() có thể giữ CPU tới socket timeout.
static unsigned long sRetryDelayMs = MQTT_RETRY_MIN_MS;

static void mqttHandleCommand(char* topic, byte* payload, unsigned int length) {
  (void)topic;
  char cmd[96];
  const unsigned int n = length < sizeof(cmd) - 1 ? length : sizeof(cmd) - 1;
  memcpy(cmd, payload, n);
  cmd[n] = '\0';

  if (strcmp(cmd, "pump on") == 0) {
    setPumpAuto(false);
    if (sys.gWater >= (int)sys.waterThresholdLow) setRelay(true);
  } else if (strcmp(cmd, "pump off") == 0) {
    setPumpAuto(false);
    setRelay(false);
  } else if (strcmp(cmd, "fan on") == 0) {
    setFanAuto(false);
    setFan(true);
  } else if (strcmp(cmd, "fan off") == 0) {
    setFanAuto(false);
    setFan(false);
  } else if (strcmp(cmd, "led on") == 0) {
    setLightAuto(false);
    setBlueLed(true);
  } else if (strcmp(cmd, "led off") == 0) {
    setLightAuto(false);
    setBlueLed(false);
  } else if (strcmp(cmd, "windmill on") == 0) {
    setWindmill(true);
  } else if (strcmp(cmd, "windmill off") == 0) {
    setWindmill(false);
  } else if (strcmp(cmd, "feed") == 0) {
    startFeed();
  } else if (strcmp(cmd, "alarm on") == 0) {
    setManualAlarm(true);
  } else if (strcmp(cmd, "alarm off") == 0) {
    setManualAlarm(false);
  } else if (strncmp(cmd, "rgb ", 4) == 0) {
    int r = 0, g = 0, b = 0;
    if (sscanf(cmd + 4, "%d %d %d", &r, &g, &b) == 3) {
      setRgbAuto(false);
      setRgb((uint8_t)constrain(r, 0, 255), (uint8_t)constrain(g, 0, 255),
             (uint8_t)constrain(b, 0, 255));
    }
  } else {
    LOG_WARN("MQTT: unknown command");
    return;
  }
  LOG_INFO("MQTT: command applied");
}
#endif

void mqttInit() {
  sConnected = false;
#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
  if (MQTT_BROKER[0] == '\0') {
    LOG_WARN("MQTT enabled but MQTT_BROKER is empty");
    return;
  }
#if MQTT_USE_TLS
  // sMqtt.connect() dùng NetworkClient::connect(host, port) — chỉ giới hạn phần
  // TCP; bắt tay TLS có handshake_timeout riêng (core mặc định tới 120 s).
  sNetClient.setHandshakeTimeout(MQTT_HANDSHAKE_TIMEOUT_S);
  if (MQTT_TLS_INSECURE) {
    sNetClient.setInsecure();
    LOG_WARN("MQTT TLS: certificate verification disabled");
  } else if (MQTT_CA_CERT[0] != '\0') {
    sNetClient.setCACert(MQTT_CA_CERT);
  } else {
    LOG_ERROR_CODE(601, "MQTT TLS without CA and insecure disabled");
    return;
  }
#endif
  snprintf(sClientId, sizeof(sClientId), "smartfarm-%04X",
           (unsigned)(ESP.getEfuseMac() & 0xFFFF));
  sMqtt.setServer(MQTT_BROKER, MQTT_PORT);
  sMqtt.setCallback(mqttHandleCommand);
  sMqtt.setBufferSize(512);
  // Mặc định của thư viện là 15 s: một lần broker im lặng là loop() đứng 15 s.
  sMqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  // PubSubClient::connect() gọi _client->connect(domain, port) KHÔNG kèm
  // timeout, nên socketTimeout ở trên chỉ chặn lúc chờ CONNACK/publish. Phần
  // TCP connect dùng _timeout của client (core 3.x mặc định 3 s), phải đặt qua
  // API riêng của từng core. DNS vẫn do stack WiFi tự giới hạn.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  sNetClient.setConnectionTimeout((uint32_t)MQTT_SOCKET_TIMEOUT_S * 1000UL);
#else
  // core 2.x: connect(host, port) dùng _timeout mà setTimeout() đặt.
  sNetClient.setTimeout((unsigned long)MQTT_SOCKET_TIMEOUT_S * 1000UL);
#endif
  sRetryDelayMs = MQTT_RETRY_MIN_MS;
  LOG_INFO("MQTT init");
#else
  LOG_DEBUG("MQTT disabled");
#endif
}

#if MQTT_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
static void publishState() {
  // JSON chỉ hợp lệ với số hữu hạn; NaN sẽ in ra "nan" (không parse được) và
  // làm dashboard/broker nhận dữ liệu rác.
  const float temp = isfinite(sys.gTemp) ? sys.gTemp : 0.0f;
  const float hum = isfinite(sys.gHum) ? sys.gHum : 0.0f;
  char payload[384];
  snprintf(payload, sizeof(payload),
           "{\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,"
           "\"relay\":%s,\"fan\":%s,\"led\":%s,\"windmill\":%s,\"alarm\":%s,"
           "\"pumpAuto\":%s,\"lightAuto\":%s,\"rgbAuto\":%s,\"fanAuto\":%s}",
           (double)temp, (double)hum, sys.gLight, sys.gSoil, sys.gWater,
           sys.relayState ? "true" : "false", sys.fanState ? "true" : "false",
           sys.blueLedState ? "true" : "false", sys.windmillOn ? "true" : "false",
           manualAlarm() ? "true" : "false",
           sys.pumpAuto ? "true" : "false", sys.lightAuto ? "true" : "false",
           sys.rgbAuto ? "true" : "false", sys.fanAuto ? "true" : "false");
  char topic[64];
  snprintf(topic, sizeof(topic), "%s/state", MQTT_TOPIC_PREFIX);
  sMqtt.publish(topic, payload);
  sLastPublish = millis();
}
#endif

void mqttLoop() {
#if MQTT_ENABLED
#if defined(SMARTFARM_TEST_HOOKS)
  sConnected = true;
#else
  // wifiConnected() dùng trạng thái WiFi đã cache cho loopTask — không gọi
  // driver WiFi mỗi vòng loop như WiFi.status() trước đây.
  if (MQTT_BROKER[0] == '\0' || !wifiConnected()) {
    sConnected = false;
    return;
  }
  if (!sMqtt.connected()) {
    sConnected = false;
    const unsigned long now = millis();
    if (now - sLastAttempt < sRetryDelayMs) return;
    sLastAttempt = now;

    char statusTopic[64];
    snprintf(statusTopic, sizeof(statusTopic), "%s/status", MQTT_TOPIC_PREFIX);
    bool ok;
    if (MQTT_USER[0] != '\0') {
      ok = sMqtt.connect(sClientId, MQTT_USER, MQTT_PASSWORD, statusTopic, 0, true, "offline");
    } else {
      ok = sMqtt.connect(sClientId, statusTopic, 0, true, "offline");
    }
    sConnected = ok;
    if (ok) {
      sRetryDelayMs = MQTT_RETRY_MIN_MS;
      char cmdTopic[64];
      snprintf(cmdTopic, sizeof(cmdTopic), "%s/cmd", MQTT_TOPIC_PREFIX);
      sMqtt.subscribe(cmdTopic);
      sMqtt.publish(statusTopic, "online", true);
      sLastPublish = 0;
      LOG_INFO("MQTT connected");
    } else {
      // Giãn cách dần: 5 s → 10 s → 20 s → 40 s → 60 s (tối đa).
      sRetryDelayMs = (sRetryDelayMs >= MQTT_RETRY_MAX_MS / 2) ? MQTT_RETRY_MAX_MS
                                                             : sRetryDelayMs * 2;
    }
  } else {
    sMqtt.loop();
    sConnected = true;
    if (millis() - sLastPublish >= MQTT_PUBLISH_INTERVAL_MS) publishState();
  }
#endif
#else
  sConnected = false;
#endif
}

bool mqttConnected() { return sConnected; }
