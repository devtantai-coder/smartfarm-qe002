#include "globals.h"
#include "logger.h"
#include "config.h"
#include "settings.h"

// ── Bản chụp giá trị đã ghi flash ──
// NVS chỉ chịu được số lần ghi hữu hạn; UI có thể bấm nút liên tục nên
// saveSettings() chỉ put các khóa THỰC SỰ đổi (mỗi put là một lần commit flash).
// Mã bảo mật (webToken/otaPassword) KHÔNG được ghi flash dạng plaintext;
// chúng chỉ tồn tại ở firmware (compile-time) trong RAM.
namespace {
struct SettingsSnapshot {
  bool valid;
  float soilLow, soilHigh, waterLow;
  int lightThr;
  float tempHigh, tempLow;
  bool pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto;
  char ssid[32], wifiPass[64];
};
SettingsSnapshot sLast = {};
}  // namespace

static bool sameCStr(const char* a, const char* b) { return strcmp(a, b) == 0; }

// Đọc float từ NVS. Giá trị không hữu hạn (flash hỏng/bị ghi lỗi NaN/Inf)
// quay về mặc định để logic so ngưỡng không rơi vào hành vi không xác định.
static float loadFloat(const char* key, float dflt) {
  float v = preferences.getFloat(key, dflt);
  return isfinite(v) ? v : dflt;
}

// Ghi lại các giá trị được lưu flash làm mốc so sánh cho lần saveSettings() sau.
static void captureSnapshot() {
  sLast.valid = true;
  sLast.soilLow = sys.soilThresholdLow;   sLast.soilHigh = sys.soilThresholdHigh;
  sLast.waterLow = sys.waterThresholdLow; sLast.lightThr = sys.lightThreshold;
  sLast.tempHigh = sys.tempThresholdHigh; sLast.tempLow = sys.tempThresholdLow;
  sLast.pumpAuto = sys.pumpAuto; sLast.lightAuto = sys.lightAuto;
  sLast.rgbAuto = sys.rgbAuto;   sLast.fanAuto = sys.fanAuto;
  sLast.breatheAuto = sys.breatheAuto;
  strncpy(sLast.ssid, sys.ssid, sizeof(sLast.ssid) - 1);
  sLast.ssid[sizeof(sLast.ssid) - 1] = '\0';
  strncpy(sLast.wifiPass, sys.password, sizeof(sLast.wifiPass) - 1);
  sLast.wifiPass[sizeof(sLast.wifiPass) - 1] = '\0';
}

void saveSettings() {
  bool writeAll = !sLast.valid;   // lần lưu đầu sau khi mở nguồn / reset: ghi tất cả

  bool changed =
      writeAll ||
      sLast.soilLow != sys.soilThresholdLow || sLast.soilHigh != sys.soilThresholdHigh ||
      sLast.waterLow != sys.waterThresholdLow || sLast.lightThr != sys.lightThreshold ||
      sLast.tempHigh != sys.tempThresholdHigh || sLast.tempLow != sys.tempThresholdLow ||
      sLast.pumpAuto != sys.pumpAuto || sLast.lightAuto != sys.lightAuto ||
      sLast.rgbAuto != sys.rgbAuto || sLast.fanAuto != sys.fanAuto ||
      sLast.breatheAuto != sys.breatheAuto ||
      !sameCStr(sLast.ssid, sys.ssid) || !sameCStr(sLast.wifiPass, sys.password);

  if (!changed) {
    LOG_DEBUG("Settings unchanged, skip flash write");
    return;
  }

  preferences.begin("smartfarm", false);
  if (writeAll || sLast.soilLow != sys.soilThresholdLow)   preferences.putFloat("soilLow", sys.soilThresholdLow);
  if (writeAll || sLast.soilHigh != sys.soilThresholdHigh) preferences.putFloat("soilHigh", sys.soilThresholdHigh);
  if (writeAll || sLast.waterLow != sys.waterThresholdLow) preferences.putFloat("waterLow", sys.waterThresholdLow);
  if (writeAll || sLast.lightThr != sys.lightThreshold)    preferences.putInt("lightThr", sys.lightThreshold);
  if (writeAll || sLast.tempHigh != sys.tempThresholdHigh) preferences.putFloat("tempHigh", sys.tempThresholdHigh);
  if (writeAll || sLast.tempLow != sys.tempThresholdLow)   preferences.putFloat("tempLow", sys.tempThresholdLow);
  if (writeAll || sLast.pumpAuto != sys.pumpAuto)          preferences.putBool("pumpAuto", sys.pumpAuto);
  if (writeAll || sLast.lightAuto != sys.lightAuto)        preferences.putBool("lightAuto", sys.lightAuto);
  if (writeAll || sLast.rgbAuto != sys.rgbAuto)            preferences.putBool("rgbAuto", sys.rgbAuto);
  if (writeAll || sLast.fanAuto != sys.fanAuto)            preferences.putBool("fanAuto", sys.fanAuto);
  if (writeAll || sLast.breatheAuto != sys.breatheAuto)    preferences.putBool("breatheAuto", sys.breatheAuto);
  if (writeAll || !sameCStr(sLast.ssid, sys.ssid))         preferences.putString("wifiSsid", sys.ssid);
  if (writeAll || !sameCStr(sLast.wifiPass, sys.password)) preferences.putString("wifiPass", sys.password);
  preferences.end();

  captureSnapshot();
  LOG_INFO("Settings saved to flash");
}

void loadSettings() {
  preferences.begin("smartfarm", true);
  sys.soilThresholdLow  = loadFloat("soilLow", 400.0f);
  sys.soilThresholdHigh = loadFloat("soilHigh", 700.0f);
  sys.waterThresholdLow = loadFloat("waterLow", 200.0f);
  sys.lightThreshold    = preferences.getInt("lightThr", 500);
  sys.tempThresholdHigh = loadFloat("tempHigh", 30.0f);
  sys.tempThresholdLow  = loadFloat("tempLow", 25.0f);
  sys.pumpAuto          = preferences.getBool("pumpAuto", true);
  sys.lightAuto         = preferences.getBool("lightAuto", true);
  sys.rgbAuto           = preferences.getBool("rgbAuto", true);
  sys.fanAuto           = preferences.getBool("fanAuto", true);
  sys.breatheAuto       = preferences.getBool("breatheAuto", false);  // chỉ bật khi người dùng chọn (hoạt động 3)

  // WiFi credentials: ưu tiên giá trị đã lưu, fallback về giá trị compile-time
  String savedSsid = preferences.getString("wifiSsid", "");
  String savedPass = preferences.getString("wifiPass", "");
  if (savedSsid.length() > 0) {
    savedSsid.toCharArray(sys.ssid, sizeof(sys.ssid));
  }
  if (savedPass.length() > 0) {
    savedPass.toCharArray(sys.password, sizeof(sys.password));
  }

  preferences.end();
  // Chụp lại giá trị vừa đọc: các lần saveSettings() sau chỉ ghi khóa đổi.
  captureSnapshot();
  LOG_INFO("Settings loaded from flash");
}

void resetSettings() {
  preferences.begin("smartfarm", false);
  preferences.clear();
  preferences.end();
  sLast.valid = false;   // flash đã xóa: lần lưu sau phải ghi lại toàn bộ

  // Mặc định lấy từ chính SystemState để chỉ có một nguồn sự thật.
  const SystemState defaults;
  sys.soilThresholdLow  = defaults.soilThresholdLow;
  sys.soilThresholdHigh = defaults.soilThresholdHigh;
  sys.waterThresholdLow = defaults.waterThresholdLow;
  sys.lightThreshold    = defaults.lightThreshold;
  sys.tempThresholdHigh = defaults.tempThresholdHigh;
  sys.tempThresholdLow  = defaults.tempThresholdLow;
  sys.pumpAuto          = defaults.pumpAuto;
  sys.lightAuto         = defaults.lightAuto;
  sys.rgbAuto           = defaults.rgbAuto;
  sys.fanAuto           = defaults.fanAuto;
  sys.breatheAuto       = defaults.breatheAuto;
  strncpy(sys.ssid, defaults.ssid, sizeof(sys.ssid) - 1);
  sys.ssid[sizeof(sys.ssid) - 1] = '\0';
  strncpy(sys.password, defaults.password, sizeof(sys.password) - 1);
  sys.password[sizeof(sys.password) - 1] = '\0';
  strncpy(sys.webToken, WEB_TOKEN, sizeof(sys.webToken) - 1);
  sys.webToken[sizeof(sys.webToken) - 1] = '\0';
  strncpy(sys.otaPassword, OTA_PASSWORD, sizeof(sys.otaPassword) - 1);
  sys.otaPassword[sizeof(sys.otaPassword) - 1] = '\0';
  sys.gLastNonce = 0;
  sys.gNeedRestart = false;
  LOG_INFO("Settings reset to defaults");
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: quên bản chụp đã lưu (mock xóa flash giữa các test).
void testResetSettingsStatics() { sLast.valid = false; }
#endif
