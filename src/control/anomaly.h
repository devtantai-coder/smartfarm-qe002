#ifndef SMARTFARM_ANOMALY_H
#define SMARTFARM_ANOMALY_H
#include <Arduino.h>
void anomalyInit();
void anomalyUpdate();
float anomalyZScore(float value, float mean, float stdDev);
bool anomalyIsHealthy(const char* sensor);
float anomalyScore(const char* sensor);
#endif
