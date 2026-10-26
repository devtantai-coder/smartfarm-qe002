#include "../health.h"
#include "../calibration.h"
#include "../globals.h"
#include "../logger.h"
#include <cstdio>

bool healthCheck() {
  return sys.sensorHealthy;
}

const char* healthStatus() {
  return sys.sensorHealthy ? "ok" : "degraded";
}

void handleHealth() {
  server.send(healthCheck() ? 200 : 503, "text/plain", healthStatus());
}


void handleCalibrate() {
  // Minimal contract: accept ?sensor=temp&offset=1.2 and persist.
  if (server.hasArg("sensor") && server.hasArg("offset")) {
    String sensorArg = server.arg("sensor");
    const char* sensor = sensorArg.c_str();
    float off = server.arg("offset").toFloat();
    calibrationSetOffset(sensor, off);
    calibrationSave();
    server.send(200, "text/plain", "calibrated");
  } else {
    server.send(400, "text/plain", "missing args");
  }
}
