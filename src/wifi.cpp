#include "globals.h"
#include "logger.h"
#include "config.h"
#include "wifi.h"

// Kết nối lại cho MỌI trạng thái mất kết nối (disconnected, idle, scan, failed, không có SSID...)
// và chỉ ghi log một lần mỗi lần mất kết nối để tránh spam serial.
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
