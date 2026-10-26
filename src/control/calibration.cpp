#include "../calibration.h"
#include "../globals.h"
#include "../logger.h"
#include <Preferences.h>

static Preferences sPrefs;

void calibrationInit() { calibrationLoad(); }

float calibrationApplyTemp(float raw) { return (raw + sys.calTempOffset) * sys.calTempGain; }
float calibrationApplyHum(float raw) { return (raw + sys.calHumOffset) * sys.calHumGain; }
float calibrationApplyLight(int raw) { return (float)(raw + sys.calLightOffset); }
float calibrationApplySoil(int raw) { return (float)(raw + sys.calSoilOffset); }
float calibrationApplyWater(int raw) { return (float)(raw + sys.calWaterOffset); }

static bool validOffset(float offset, float min, float max) {
  return isfinite(offset) && offset >= min && offset <= max;
}

bool calibrationSetOffset(const char* sensor, float offset) {
  if (!sensor || !isfinite(offset)) return false;
  if (strcmp(sensor, "temp") == 0) {
    if (!validOffset(offset, -20.0f, 20.0f)) return false;
    sys.calTempOffset = offset;
    return true;
  }
  if (strcmp(sensor, "hum") == 0) {
    if (!validOffset(offset, -50.0f, 50.0f)) return false;
    sys.calHumOffset = offset;
    return true;
  }
  if (strcmp(sensor, "light") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calLightOffset = (int)offset;
    return true;
  }
  if (strcmp(sensor, "soil") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calSoilOffset = (int)offset;
    return true;
  }
  if (strcmp(sensor, "water") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calWaterOffset = (int)offset;
    return true;
  }
  return false;
}

void calibrationSave() {
  sPrefs.begin("calib", false);
  sPrefs.putFloat("tOff", sys.calTempOffset);
  sPrefs.putFloat("tGain", sys.calTempGain);
  sPrefs.putFloat("hOff", sys.calHumOffset);
  sPrefs.putFloat("hGain", sys.calHumGain);
  sPrefs.putInt("lOff", sys.calLightOffset);
  sPrefs.putInt("sOff", sys.calSoilOffset);
  sPrefs.putInt("wOff", sys.calWaterOffset);
  sPrefs.end();
  LOG_DEBUG("Calibration saved");
}

void calibrationLoad() {
  sPrefs.begin("calib", true);
  sys.calTempOffset = sPrefs.getFloat("tOff", 0.0f);
  if (!validOffset(sys.calTempOffset, -20.0f, 20.0f)) sys.calTempOffset = 0.0f;
  sys.calTempGain = sPrefs.getFloat("tGain", 1.0f);
  if (!isfinite(sys.calTempGain) || sys.calTempGain < 0.1f || sys.calTempGain > 10.0f) sys.calTempGain = 1.0f;
  sys.calHumOffset = sPrefs.getFloat("hOff", 0.0f);
  if (!validOffset(sys.calHumOffset, -50.0f, 50.0f)) sys.calHumOffset = 0.0f;
  sys.calHumGain = sPrefs.getFloat("hGain", 1.0f);
  if (!isfinite(sys.calHumGain) || sys.calHumGain < 0.1f || sys.calHumGain > 10.0f) sys.calHumGain = 1.0f;
  sys.calLightOffset = constrain(sPrefs.getInt("lOff", 0), -4095, 4095);
  sys.calSoilOffset = constrain(sPrefs.getInt("sOff", 0), -4095, 4095);
  sys.calWaterOffset = constrain(sPrefs.getInt("wOff", 0), -4095, 4095);
  sPrefs.end();
}
