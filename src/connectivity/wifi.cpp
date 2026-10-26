#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../wifi.h"

// Kết nối lại cho MỌI trạng thái mất kết nối (disconnected, idle, scan, failed, không có SSID...)
// và chỉ ghi log một lần mỗi lần mất kết nối để tránh spam serial.
static bool sWarned = false;      // đã ghi log mất WiFi cho đợt này chưa
static bool sAnnounced = false;   // đã in IP lần đầu sau khi kết nối chưa
static bool sBootTimedOut = false;
static unsigned long sBootMs = 0; // mốc bắt đầu tính timeout khởi động

// Trạng thái kết nối cache lại.
// WiFi.status() gọi vào driver WiFi qua khóa toàn cục — khóa đó cũng do chính
// stack WiFi dùng khi xử lý gói tin. Hỏi ở tần số của loop() (~1 kHz) sẽ
// tranh giành khóa với stack WiFi mà không tăng thêm giá trị gì: trạng thái
// kết nối thay đổi theo giây chứ không theo mili-giây. Hỏi mỗi
// WIFI_STATUS_POLL_MS là đủ để phản ứng kịp (nhánh thử lại cứ 10 s).
static int sLastStatus = WL_DISCONNECTED;
static unsigned long sStatusAt = 0;
static bool sStatusValid = false;

static int wifiStatusCached(unsigned long now) {
  if (!sStatusValid || (now - sStatusAt >= WIFI_STATUS_POLL_MS)) {
    sLastStatus = WiFi.status();
    sStatusAt = now;
    sStatusValid = true;
  }
  return sLastStatus;
}

void manageWiFi() {
  unsigned long now = millis();
  if (sBootMs == 0) sBootMs = now;   // lần gọi đầu tiên ngay sau setup()

  if (wifiStatusCached(now) == WL_CONNECTED) {
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
  sStatusValid = false;      // ép hỏi lại trạng thái ở lần gọi kế tiếp
}
#endif

bool wifiConnected() {
  // Use cached status to avoid hammering WiFi.status() driver.
  return wifiStatusCached(millis()) == WL_CONNECTED;
}
