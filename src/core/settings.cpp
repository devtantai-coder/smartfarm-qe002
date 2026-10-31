#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../settings.h"
#include "../calibration.h"

// ── Bản chụp giá trị đã ghi flash ──
// NVS chỉ chịu được số lần ghi hữu hạn; UI có thể bấm nút liên tục nên
// saveSettings() chỉ put các khóa THỰC SỰ đổi (mỗi put là một lần commit flash).
//
// Về bảo mật: webToken/otaPassword là hằng compile-time, KHÔNG bao giờ ghi
// xuống flash — chúng chỉ sống trong RAM. Ngược lại SSID/mật khẩu WiFi CÓ được
// lưu vào NVS (namespace "smartfarm") vì tính năng /wifi cần nó sau khi khởi
// động lại. NVS KHÔNG mã hóa: ai đọc được flash (esptool read_flash, hoặc nạp
// sketch khác vào cùng board) đều đọc được PSK. Nếu điều đó không chấp nhận
// được trong môi trường của bạn, hãy bỏ trống mật khẩu và nạp lại firmware.
namespace {
struct SettingsSnapshot {
  bool valid;
  float soilLow, soilHigh, waterLow;
  int lightThr;
  float tempHigh, tempLow;
  bool pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto;
  bool powerSave;
  uint32_t sleepSeconds;
  // Kích thước lấy theo đúng buffer trong SystemState: khai báo cứng số 32 sẽ
  // cắt SSID 32 octet xuống 31, khiến so sánh "chỉ ghi khóa đổi" luôn sai và
  // ghi lại y nguyên giá trị cũ mỗi lần lưu.
  char ssid[sizeof(sys.ssid)], wifiPass[sizeof(sys.password)];
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
  sLast.powerSave = sys.powerSave;
  sLast.sleepSeconds = sys.sleepSeconds;
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
      sLast.powerSave != sys.powerSave || sLast.sleepSeconds != sys.sleepSeconds ||
      !sameCStr(sLast.ssid, sys.ssid) || !sameCStr(sLast.wifiPass, sys.password);

  if (!changed) {
    LOG_DEBUG("Settings unchanged, skip flash write");
    return;
  }

  // begin() trả false khi mở NVS hỏng (partition lỗi/full): khi đó mọi put* âm
  // thầm mất tác dụng. Phải trả về TRƯỚC captureSnapshot — nếu chụp, bản "đã
  // lưu" giả này khiến lần sau bỏ qua đúng những khóa chưa bao giờ xuống flash.
  if (!preferences.begin("smartfarm", false)) {
    LOG_ERROR("NVS open failed (write) - settings NOT saved");
    return;
  }
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
  if (writeAll || sLast.powerSave != sys.powerSave)        preferences.putBool("powerSave", sys.powerSave);
  if (writeAll || sLast.sleepSeconds != sys.sleepSeconds)  preferences.putInt("sleepSec", (int)sys.sleepSeconds);
  if (writeAll || !sameCStr(sLast.ssid, sys.ssid))         preferences.putString("wifiSsid", sys.ssid);
  if (writeAll || !sameCStr(sLast.wifiPass, sys.password)) preferences.putString("wifiPass", sys.password);
  preferences.end();

  captureSnapshot();
  LOG_INFO("Settings saved to flash");
}

void loadSettings() {
  // Không mở được NVS thì mọi get* trả default của thư viện một cách im lặng:
  // giữ nguyên mặc định firmware, và đặt bản chụp vô hiệu để lần lưu đầu sau
  // này ghi lại ĐỦ mọi khóa (chưa biết gì trong flash).
  if (!preferences.begin("smartfarm", true)) {
    LOG_ERROR("NVS open failed (read) - using firmware defaults");
    sLast.valid = false;
    return;
  }
  // Khoảng giá trị hợp lệ: ngoài khoảng này (flash bị hỏng, hoặc ghi tay sai)
  // thì logic so ngưỡng sẽ hành xử sai — ví dụ soilHigh < soilLow thì bơm
  // không bao giờ chạy. Constrain ngay khi đọc lên.
  sys.soilThresholdLow  = constrain(loadFloat("soilLow", 400.0f), 0.0f, 4095.0f);
  sys.soilThresholdHigh = constrain(loadFloat("soilHigh", 700.0f), 0.0f, 4095.0f);
  sys.waterThresholdLow = constrain(loadFloat("waterLow", 200.0f), 0.0f, 4095.0f);
  sys.lightThreshold    = constrain(preferences.getInt("lightThr", 500), 0, 4095);
  sys.tempThresholdHigh = constrain(loadFloat("tempHigh", 30.0f), 0.0f, 60.0f);
  sys.tempThresholdLow  = constrain(loadFloat("tempLow", 25.0f), 0.0f, 60.0f);

  // Kẹp từng giá trị riêng lẻ chưa đủ: ảnh NVS hỏng / ghi tay / từ firmware cũ
  // vẫn có thể để lại cặp ngược (high <= low), và controlPump()/controlFan()
  // bỏ qua luôn khi thấy cặp ngược -> tưới/tự động im lặng dừng tới lần boot
  // sau. Dựng lại bất biến high > low y như cách /threshold làm khi ghi.
  if (sys.soilThresholdHigh <= sys.soilThresholdLow) {
    sys.soilThresholdHigh = sys.soilThresholdLow + 50.0f;      // ưu tiên giữ ngưỡng thấp
    if (sys.soilThresholdHigh > 4095.0f) {                     // sát trần: hạ thấp để còn khoảng
      sys.soilThresholdHigh = 4095.0f;
      sys.soilThresholdLow = 4045.0f;
    }
  }
  if (sys.tempThresholdHigh <= sys.tempThresholdLow) {
    sys.tempThresholdHigh = sys.tempThresholdLow + 1.0f;
    if (sys.tempThresholdHigh > 60.0f) {
      sys.tempThresholdHigh = 60.0f;
      sys.tempThresholdLow = 59.0f;
    }
  }
  sys.pumpAuto          = preferences.getBool("pumpAuto", true);
  sys.lightAuto         = preferences.getBool("lightAuto", true);
  sys.rgbAuto           = preferences.getBool("rgbAuto", true);
  sys.fanAuto           = preferences.getBool("fanAuto", true);
  sys.breatheAuto       = preferences.getBool("breatheAuto", false);  // chỉ bật khi người dùng chọn (hoạt động 3)
  sys.powerSave         = preferences.getBool("powerSave", false);
  sys.sleepSeconds      = constrain(preferences.getInt("sleepSec", DEFAULT_SLEEP_SECONDS), 1, 86400);

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
  if (!preferences.begin("smartfarm", false)) {
    LOG_ERROR("NVS open failed (reset) - only RAM defaults restored");
  } else {
    preferences.clear();
    preferences.end();
  }
  // "Về mặc định" phải bao gồm cả hiệu chuẩn: offset/gain nằm ở namespace
  // "calib" nên /reset trước đây khôi phục ngưỡng nhưng giữ thầm độ lệch
  // ADC/nhiệt-ẩm. calibrationLoad() nạp lại mặc định từ flash vừa xóa và cập
  // nhật bản chụp riêng của nó, nên calibrationSave() kế tiếp không ghi thừa.
  static Preferences calibPrefs;
  if (!calibPrefs.begin("calib", false)) {
    LOG_ERROR("NVS open failed (calib) - calibration kept");
  } else {
    calibPrefs.clear();
    calibPrefs.end();
  }
  calibrationLoad();
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
  sys.powerSave         = defaults.powerSave;
  sys.sleepSeconds      = defaults.sleepSeconds;
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
