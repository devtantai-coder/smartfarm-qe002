#include "anomaly.h"
#include "../core/globals.h"
#include "../core/logger.h"
#include <cmath>

static const uint8_t N = 10;
static float sTemp[N] = {0}, sHum[N] = {0};
static uint8_t sIdx = 0;
static bool sFilled = false;

void anomalyInit() {
  sys.anomalyTemp = 0.0f;
  sys.anomalyHum = 0.0f;
  sys.sensorHealthyTemp = true;
  sys.sensorHealthyHum = true;
  sIdx = 0; sFilled = false;
}

static void stats(const float* buf, uint8_t n, float& mean, float& sd) {
  if (n == 0) { mean = 0; sd = 1; return; }
  float s = 0;
  for (uint8_t i = 0; i < n; i++) s += buf[i];
  mean = s / n;
  float v = 0;
  for (uint8_t i = 0; i < n; i++) { float d = buf[i] - mean; v += d * d; }
  sd = sqrtf(v / n);
  if (sd < 0.001f) sd = 0.001f;
}

void anomalyUpdate() {
  sTemp[sIdx] = sys.gTemp;
  sHum[sIdx] = sys.gHum;
  sIdx = (sIdx + 1) % N;
  if (sIdx == 0) sFilled = true;
  uint8_t n = sFilled ? N : sIdx;
  if (n == 0) n = 1;

  float tMean, tSd, hMean, hSd;
  stats(sTemp, n, tMean, tSd);
  stats(sHum, n, hMean, hSd);
  sys.anomalyTemp = (sys.gTemp - tMean) / tSd;
  sys.anomalyHum = (sys.gHum - hMean) / hSd;

  // Tổng hợp: range + z-score + failure count từ readAllSensors.
  // Không ghi đè health do NaN ngay khi vừa thay bằng giá trị cũ.
  bool dhtOk = (sys.sensorFailCount == 0);
  sys.sensorHealthyTemp = dhtOk && isfinite(sys.gTemp) && sys.gTemp >= -10.0f && sys.gTemp <= 80.0f && fabsf(sys.anomalyTemp) < 4.0f;
  sys.sensorHealthyHum  = dhtOk && isfinite(sys.gHum) && sys.gHum >= 0.0f && sys.gHum <= 100.0f && fabsf(sys.anomalyHum) < 4.0f;
  sys.sensorHealthy = sys.sensorHealthyTemp && sys.sensorHealthyHum &&
                      sys.lightHealthy && sys.soilHealthy && sys.waterHealthy;
}

float anomalyZScore(float value, float mean, float stdDev) {
  if (stdDev == 0.0f) return 0.0f;
  return (value - mean) / stdDev;
}

bool anomalyIsHealthy(const char* sensor) {
  if (!sensor) return sys.sensorHealthy;
  if (strcmp(sensor, "temp") == 0) return sys.sensorHealthyTemp;
  if (strcmp(sensor, "hum") == 0) return sys.sensorHealthyHum;
  return true;
}

float anomalyScore(const char* sensor) {
  if (!sensor) return 0.0f;
  if (strcmp(sensor, "temp") == 0) return sys.anomalyTemp;
  if (strcmp(sensor, "hum") == 0) return sys.anomalyHum;
  return 0.0f;
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetAnomalyStatics() {
  // Reset bộ đệm anomaly về trạng thái ban đầu để test độc lập.
  for (uint8_t i = 0; i < 10; i++) { sTemp[i] = 0.0f; sHum[i] = 0.0f; }
  sIdx = 0;
  sFilled = false;
  anomalyInit();
}
#endif
