#include "../metrics.h"
#include "../globals.h"
#include "../logger.h"
#include "../webserver.h"
#include <cstdio>

static unsigned long sStart = 0;
static char sBuf[320];

// Thống kê vòng loop: trung bình/lớn nhất để đo tác động của tối ưu.
static uint32_t sLoops = 0;
static uint64_t sLoopUsTotal = 0;
static uint32_t sLoopUsMax = 0;

void metricsInit() {
  sStart = millis();
  sLoops = 0;
  sLoopUsTotal = 0;
  sLoopUsMax = 0;
}

void metricsNoteLoop(uint32_t us) {
  sLoops++;
  sLoopUsTotal += us;
  if (us > sLoopUsMax) sLoopUsMax = us;
}

void metricsCollect() {
  unsigned long freeHeap = 0;
  unsigned long minFreeHeap = 0;
#ifndef SMARTFARM_TEST_HOOKS
  freeHeap = ESP.getFreeHeap();
  minFreeHeap = ESP.getMinFreeHeap();
#endif
  const unsigned long loopAvgUs =
      sLoops ? (unsigned long)(sLoopUsTotal / sLoops) : 0;
  snprintf(sBuf, sizeof(sBuf),
           "{\"uptime_ms\":%lu,\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,\"healthy\":%s,\"free_heap\":%lu,\"min_free_heap\":%lu,\"loops\":%lu,\"loop_us_avg\":%lu,\"loop_us_max\":%lu}",
           (unsigned long)(millis() - sStart), sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater,
           sys.sensorHealthy ? "true" : "false", freeHeap, minFreeHeap,
           (unsigned long)sLoops, loopAvgUs, (unsigned long)sLoopUsMax);
}

const char* metricsToJson() { return sBuf; }

unsigned long metricsUptime() { return millis() - sStart; }


void handleMetrics() {
  if (!webTokenGuard()) return;
  metricsCollect();
  server.send(200, "application/json", metricsToJson());
}
