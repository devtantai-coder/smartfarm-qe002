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
  breatheAuto       = preferences.getBool("breatheAuto", false);  // opt-in (activity 3)
  preferences.end();
  LOG_INFO("Settings loaded from flash");
}
