#include "globals.h"
#include "logger.h"
#include "config.h"
#include "wifi.h"

// Reconnect for ANY lost state (disconnected, idle, scan, failed, no SSID...)
// and log only once per outage to avoid serial spam.
void manageWiFi() {
  static bool sWarned = false;
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (sWarned) {
      sWarned = false;
      LOG_INFO("WiFi reconnected");
    }
    lastWifiCheck = now;
    return;
  }

  if (now - lastWifiCheck < WIFI_CHECK_INTERVAL) return;

  if (!sWarned) {
    LOG_ERROR_CODE(401, "WiFi lost, reconnecting...");
    sWarned = true;
  }
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  lastWifiCheck = now;
}
