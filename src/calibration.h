#ifndef SMARTFARM_CALIBRATION_H
#define SMARTFARM_CALIBRATION_H
#include <Arduino.h>
void calibrationInit();
float calibrationApplyTemp(float raw);
float calibrationApplyHum(float raw);
float calibrationApplyLight(int raw);
float calibrationApplySoil(int raw);
float calibrationApplyWater(int raw);
bool calibrationSetOffset(const char* sensor, float offset);
void calibrationSave();
void calibrationLoad();

#ifdef SMARTFARM_TEST_HOOKS
// Quên bản chụp đã ghi flash (mock xóa NVS giữa các test).
void testResetCalibStatics();
#endif
#endif
