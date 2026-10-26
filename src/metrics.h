#ifndef SMARTFARM_METRICS_H
#define SMARTFARM_METRICS_H
#include <Arduino.h>
void metricsInit();
void metricsCollect();
const char* metricsToJson();
unsigned long metricsUptime();
void handleMetrics();
#endif
