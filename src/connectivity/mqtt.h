#ifndef SMARTFARM_MQTT_H
#define SMARTFARM_MQTT_H
#include <Arduino.h>

void mqttInit();
void mqttLoop();
bool mqttPublish(const char* topic, const char* payload);
void mqttSubscribe(const char* topic);
bool mqttConnected();

// Cấu hình broker (host/port). Nếu không gọi, MQTT tắt (không thử kết nối).
void mqttSetBroker(const char* host, uint16_t port);
const char* mqttBrokerHost();
uint16_t mqttBrokerPort();

#endif
