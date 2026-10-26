#ifndef SMARTFARM_HEALTH_H
#define SMARTFARM_HEALTH_H
#include <Arduino.h>
bool healthCheck();
const char* healthStatus();
void handleHealth();
#endif
