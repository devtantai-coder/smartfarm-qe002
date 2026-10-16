#include "globals.h"
#include "logger.h"
#include "config.h"
#include "wifi.h"

void manageWiFi() {
  unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    lastWifiCheck = now;
    return;
  }
  // Also reconnect on intermediate states (scanning, disconnected)
  if (WiFi.status() == WL_IDLE_STATUS || WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_SCAN_COMPLETED) {
    if (now - lastWifiCheck < WIFI_CHECK_INTERVAL) return;
    LOG_ERROR_CODE(401, "WiFi lost, reconnecting...");
    WiFi.reconnect();
    lastWifiCheck = now;
  }
}
