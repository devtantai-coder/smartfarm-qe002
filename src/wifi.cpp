#include "globals.h"
#include "logger.h"
#include "config.h"
#include "wifi.h"

// Kết nối lại cho MỌI trạng thái mất kết nối (disconnected, idle, scan, failed, không có SSID...)
// và chỉ ghi log một lần mỗi lần mất kết nối để tránh spam serial.
static bool sWarned = false;      // đã ghi log mất WiFi cho đợt này chưa
static bool sAnnounced = false;   // đã in IP lần đầu sau khi kết nối chưa
static bool sBootTimedOut = false;
static unsigned long sBootMs = 0; // mốc bắt đầu tính timeout khởi động

void manageWiFi() {
  unsigned long now = millis();
  if (sBootMs == 0) sBootMs = now;   // lần gọi đầu tiên ngay sau setup()

  if (WiFi.status() == WL_CONNECTED) {
    if (!sAnnounced) {
      sAnnounced = true;
      Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    }
    if (sWarned) {
      sWarned = false;
      LOG_INFO("WiFi reconnected");
    }
    sys.lastWifiCheck = now;
    return;
  }

  // Boot không chặn: quá WIFI_CONNECT_TIMEOUT mà vẫn chưa kết nối thì báo
  // lỗi 102 một lần (trước đây việc chờ nằm trong setup() dạng blocking).
  if (!sBootTimedOut && now - sBootMs >= WIFI_CONNECT_TIMEOUT) {
    sBootTimedOut = true;
    LOG_ERROR_CODE(102, "WiFi timeout - retrying in background");
  }

  if (now - sys.lastWifiCheck < WIFI_CHECK_INTERVAL) return;

  if (!sWarned) {
    LOG_ERROR_CODE(401, "WiFi lost, reconnecting...");
    sWarned = true;
  }
  WiFi.disconnect();
  WiFi.begin(sys.ssid, sys.password);
  sys.lastWifiCheck = now;
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: quên trạng thái "đã cảnh báo mất WiFi" / IP / timeout boot.
void testResetWifiStatics() {
  sWarned = false;
  sAnnounced = false;
  sBootTimedOut = false;
  sBootMs = 0;
}
#endif
