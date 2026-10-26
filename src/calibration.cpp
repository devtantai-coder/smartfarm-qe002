#include "calibration.h"
#include "globals.h"
#include "settings.h"

void calibrationInit() { calibrationLoad(); }
float calibrationApplyTemp(float raw) { return raw + sys.calTempOffset; }
float calibrationApplyHum(float raw) { return raw * sys.calHumGain; }
float calibrationApplyLight(int raw) { return (float)(raw + sys.calLightOffset); }
float calibrationApplySoil(int raw) { return (float)(raw + sys.calSoilOffset); }
float calibrationApplyWater(int raw) { return (float)(raw + sys.calWaterOffset); }
void calibrationSetOffset(const char* sensor, float offset) {
  if (!sensor) return;
  if (strcmp(sensor, "temp") == 0) sys.calTempOffset = offset;
}
void calibrationSave() {}
void calibrationLoad() {}
