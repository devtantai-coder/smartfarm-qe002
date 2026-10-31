#ifndef SMARTFARM_MQTT_H
#define SMARTFARM_MQTT_H
#include <Arduino.h>
void mqttInit();
void mqttLoop();
bool mqttConnected();
#endif
