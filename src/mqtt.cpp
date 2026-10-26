#include "mqtt.h"
#include "globals.h"
#include "logger.h"

void mqttInit() { LOG_INFO("MQTT init"); }
void mqttLoop() {}
bool mqttPublish(const char* topic, const char* payload) { (void)topic; (void)payload; return false; }
void mqttSubscribe(const char* topic) { (void)topic; }
bool mqttConnected() { return false; }
