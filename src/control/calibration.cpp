#include "calibration.h"
#include "../core/globals.h"
#include "../core/logger.h"
#include <Preferences.h>

static Preferences sPrefs;

// Snapshot file-scope (không phải function-static) để test có thể reset.
static bool sCalInit = false;
static float sCalTOff, sCalTGain, sCalHOff, sCalHGain;
static int sCalLOff, sCalSOff, sCalWOff;

void calibrationInit() { calibrationLoad(); }

float calibrationApplyTemp(float raw) { return (raw + sys.calTempOffset) * sys.calTempGain; }
float calibrationApplyHum(float raw) { return (raw + sys.calHumOffset) * sys.calHumGain; }
float calibrationApplyLight(int raw) { return (float)(raw + sys.calLightOffset); }
float calibrationApplySoil(int raw) { return (float)(raw + sys.calSoilOffset); }
float calibrationApplyWater(int raw) { return (float)(raw + sys.calWaterOffset); }

void calibrationSetOffset(const char* sensor, float offset) {
  if (!sensor) return;
  if (strcmp(sensor, "temp") == 0) sys.calTempOffset = offset;
  else if (strcmp(sensor, "hum") == 0) sys.calHumOffset = offset;
  else if (strcmp(sensor, "light") == 0) sys.calLightOffset = (int)offset;
  else if (strcmp(sensor, "soil") == 0) sys.calSoilOffset = (int)offset;
  else if (strcmp(sensor, "water") == 0) sys.calWaterOffset = (int)offset;
}

void calibrationSave() {
  // Chỉ ghi khi thực sự đổi: NVS có số lần ghi hữu hạn, gọi mỗi vòng loop
  // (như runExtras cũ) sẽ hao flash rất nhanh.
  bool changed = !sCalInit || sCalTOff != sys.calTempOffset || sCalTGain != sys.calTempGain ||
                 sCalHOff != sys.calHumOffset || sCalHGain != sys.calHumGain || sCalLOff != sys.calLightOffset ||
                 sCalSOff != sys.calSoilOffset || sCalWOff != sys.calWaterOffset;
  if (!changed) {
    return;
  }
  sPrefs.begin("calib", false);
  sPrefs.putFloat("tOff", sys.calTempOffset);
  sPrefs.putFloat("tGain", sys.calTempGain);
  sPrefs.putFloat("hOff", sys.calHumOffset);
  sPrefs.putFloat("hGain", sys.calHumGain);
  sPrefs.putInt("lOff", sys.calLightOffset);
  sPrefs.putInt("sOff", sys.calSoilOffset);
  sPrefs.putInt("wOff", sys.calWaterOffset);
  sPrefs.end();
  sCalTOff = sys.calTempOffset; sCalTGain = sys.calTempGain; sCalHOff = sys.calHumOffset; sCalHGain = sys.calHumGain;
  sCalLOff = sys.calLightOffset; sCalSOff = sys.calSoilOffset; sCalWOff = sys.calWaterOffset;
  sCalInit = true;
  LOG_DEBUG("Calibration saved");
}

void calibrationLoad() {
  sPrefs.begin("calib", true);
  sys.calTempOffset = sPrefs.getFloat("tOff", 0.0f);
  sys.calTempGain = sPrefs.getFloat("tGain", 1.0f);
  if (!isfinite(sys.calTempGain) || sys.calTempGain == 0.0f) sys.calTempGain = 1.0f;
  sys.calHumOffset = sPrefs.getFloat("hOff", 0.0f);
  sys.calHumGain = sPrefs.getFloat("hGain", 1.0f);
  if (!isfinite(sys.calHumGain) || sys.calHumGain == 0.0f) sys.calHumGain = 1.0f;
  sys.calLightOffset = sPrefs.getInt("lOff", 0);
  sys.calSoilOffset = sPrefs.getInt("sOff", 0);
  sys.calWaterOffset = sPrefs.getInt("wOff", 0);
  sPrefs.end();
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetCalibrationStatics() {
  sCalInit = false;
}
#endif
