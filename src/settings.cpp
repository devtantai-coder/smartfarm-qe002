#include "globals.h"
#include "logger.h"
#include "config.h"
#include "settings.h"

void saveSettings() {
  preferences.begin("smartfarm", false);
  preferences.putFloat("soilLow", soilThresholdLow);
  preferences.putFloat("soilHigh", soilThresholdHigh);
  preferences.putFloat("waterLow", waterThresholdLow);
  preferences.putInt("lightThr", lightThreshold);
  preferences.putFloat("tempHigh", tempThresholdHigh);
  preferences.putFloat("tempLow", tempThresholdLow);
  preferences.putBool("pumpAuto", pumpAuto);
  preferences.putBool("lightAuto", lightAuto);
  preferences.putBool("rgbAuto", rgbAuto);
  preferences.putBool("fanAuto", fanAuto);
  preferences.putBool("breatheAuto", breatheAuto);
  preferences.putString("wifiSsid", ssid);
  preferences.putString("wifiPass", password);
  preferences.putString("webToken", webToken);
  preferences.putString("otaPass", otaPassword);
  preferences.end();
  LOG_INFO("Settings saved to flash");
}

void loadSettings() {
  preferences.begin("smartfarm", true);
  soilThresholdLow  = preferences.getFloat("soilLow", 400.0f);
  soilThresholdHigh = preferences.getFloat("soilHigh", 700.0f);
  waterThresholdLow = preferences.getFloat("waterLow", 200.0f);
  lightThreshold    = preferences.getInt("lightThr", 500);
  tempThresholdHigh = preferences.getFloat("tempHigh", 30.0f);
  tempThresholdLow  = preferences.getFloat("tempLow", 25.0f);
  pumpAuto          = preferences.getBool("pumpAuto", true);
  lightAuto         = preferences.getBool("lightAuto", true);
  rgbAuto           = preferences.getBool("rgbAuto", true);
  fanAuto           = preferences.getBool("fanAuto", true);
  breatheAuto       = preferences.getBool("breatheAuto", false);  // chỉ bật khi người dùng chọn (hoạt động 3)

  // WiFi credentials: ưu tiên giá trị đã lưu, fallback về giá trị compile-time
  String savedSsid = preferences.getString("wifiSsid", "");
  String savedPass = preferences.getString("wifiPass", "");
  if (savedSsid.length() > 0) {
    savedSsid.toCharArray(ssid, sizeof(ssid));
  }
  if (savedPass.length() > 0) {
    savedPass.toCharArray(password, sizeof(password));
  }

  // Mã bảo mật: ưu tiên giá trị đã lưu, fallback về compile-time
  String savedToken = preferences.getString("webToken", "");
  String savedOtaPass = preferences.getString("otaPass", "");
  if (savedToken.length() > 0) {
    savedToken.toCharArray(webToken, sizeof(webToken));
  }
  if (savedOtaPass.length() > 0) {
    savedOtaPass.toCharArray(otaPassword, sizeof(otaPassword));
  }
  preferences.end();
  LOG_INFO("Settings loaded from flash");
}

void resetSettings() {
  preferences.begin("smartfarm", false);
  preferences.clear();
  preferences.end();
  soilThresholdLow  = 400.0f;
  soilThresholdHigh = 700.0f;
  waterThresholdLow = 200.0f;
  lightThreshold    = 500;
  tempThresholdHigh = 30.0f;
  tempThresholdLow  = 25.0f;
  pumpAuto          = true;
  lightAuto         = true;
  rgbAuto           = true;
  fanAuto           = true;
  breatheAuto       = false;
  strncpy(ssid, "YOUR_WIFI_SSID", sizeof(ssid) - 1);
  ssid[sizeof(ssid) - 1] = '\0';
  strncpy(password, "YOUR_WIFI_PASSWORD", sizeof(password) - 1);
  password[sizeof(password) - 1] = '\0';
  strncpy(webToken, WEB_TOKEN, sizeof(webToken) - 1);
  webToken[sizeof(webToken) - 1] = '\0';
  strncpy(otaPassword, OTA_PASSWORD, sizeof(otaPassword) - 1);
  otaPassword[sizeof(otaPassword) - 1] = '\0';
  gLastNonce = 0;
  gNeedRestart = false;
  LOG_INFO("Settings reset to defaults");
}
