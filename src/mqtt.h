#ifndef SMARTFARM_MQTT_H
#define SMARTFARM_MQTT_H
#include <Arduino.h>
void mqttInit();
void mqttLoop();
bool mqttPublish(const char* topic, const char* payload);
void mqttSubscribe(const char* topic);
bool mqttConnected();
#endif
