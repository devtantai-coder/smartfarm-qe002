#include "../health.h"
#include "../calibration.h"
#include "../globals.h"
#include "../logger.h"
#include "../webserver.h"
#include <cmath>
#include <cstdio>

bool healthCheck() {
  return sys.sensorHealthy;
}

const char* healthStatus() {
  return sys.sensorHealthy ? "ok" : "degraded";
}

void handleHealth() {
  if (!webTokenGuard()) return;
  server.send(healthCheck() ? 200 : 503, "text/plain", healthStatus());
}


void handleCalibrate() {
  if (!webGuard()) return;
  if (!server.hasArg("sensor") || !server.hasArg("offset")) {
    server.send(400, "text/plain", "missing args");
    return;
  }
  const String sensorArg = server.arg("sensor");
  float offset = 0.0f;
  if (!webFloatArg("offset", &offset)) {
    server.send(400, "text/plain", "missing args");
    return;
  }
  if (!calibrationSetOffset(sensorArg.c_str(), offset)) {
    server.send(400, "text/plain", "invalid sensor or offset");
    return;
  }
  calibrationSave();
  server.send(200, "text/plain", "calibrated");
}
