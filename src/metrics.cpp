#include "metrics.h"
#include "globals.h"
static unsigned long sStart = 0;
void metricsInit() { sStart = millis(); }
void metricsCollect() {}
const char* metricsToJson() { return "{}"; }
unsigned long metricsUptime() { return millis() - sStart; }
