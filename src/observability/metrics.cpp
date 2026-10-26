#include "../metrics.h"
#include "../globals.h"
#include "../logger.h"
#include <cstdio>

static unsigned long sStart = 0;
static char sBuf[256];

void metricsInit() { sStart = millis(); }

void metricsCollect() {
  unsigned long freeHeap = 0;
#ifndef SMARTFARM_TEST_HOOKS
  freeHeap = ESP.getFreeHeap();
#endif
  snprintf(sBuf, sizeof(sBuf),
           "{\"uptime_ms\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,\"healthy\":%s,\"free_heap\":%lu}",
           (unsigned long)(millis() - sStart), sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater,
           sys.sensorHealthy ? "true" : "false", freeHeap);
}

const char* metricsToJson() { return sBuf; }

unsigned long metricsUptime() { return millis() - sStart; }


void handleMetrics() {
  metricsCollect();
  server.send(200, "application/json", metricsToJson());
}
