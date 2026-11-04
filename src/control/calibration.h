#ifndef SMARTFARM_CALIBRATION_H
#define SMARTFARM_CALIBRATION_H
#include <Arduino.h>
void calibrationInit();
float calibrationApplyTemp(float raw);
float calibrationApplyHum(float raw);
float calibrationApplyLight(int raw);
float calibrationApplySoil(int raw);
float calibrationApplyWater(int raw);
void calibrationSetOffset(const char* sensor, float offset);
void calibrationSave();
void calibrationLoad();
#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: quên snapshot đã ghi để test ghi tiếp theo ghi lại.
void testResetCalibrationStatics();
#endif
#endif
