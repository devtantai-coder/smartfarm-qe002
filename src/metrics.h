#ifndef SMARTFARM_METRICS_H
#define SMARTFARM_METRICS_H
#include <Arduino.h>
void metricsInit();
void metricsCollect();
const char* metricsToJson();
unsigned long metricsUptime();
void handleMetrics();
// Ghi nhận thời gian một vòng loop() (µs) để /metrics báo trung bình/lớn nhất.
void metricsNoteLoop(uint32_t us);
#endif
