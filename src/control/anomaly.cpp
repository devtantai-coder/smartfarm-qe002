#include "../anomaly.h"
#include "../globals.h"
#include "../logger.h"
#include <cmath>

static const uint8_t N = 10;
// Cần đủ mẫu CŨ để mean/sd có nghĩa; với 1-2 mẫu, sd bị sàn 0.001 làm mọi
// thay đổi nhỏ cũng thành z khổng lồ và báo động giả ngay sau khi cấp điện.
static const uint8_t MIN_BASELINE = 5;
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
  // Chấm điểm mẫu MỚI so với các mẫu CŨ, rồi mới ghi mẫu mới vào cửa sổ.
  // Trước đây mẫu hiện tại được ghi trước khi tính mean/sd nên nó kéo mean về
  // phía mình và |z| tối đa chỉ còn (N-1)/sqrt(N) ≈ 2.85 — ngưỡng 4.0 không
  // bao giờ chạm tới, cờ "bất thường" vô dụng.
  const uint8_t n = sFilled ? N : sIdx;
  const bool haveBaseline = n >= MIN_BASELINE;
  float tMean = 0.0f, tSd = 1.0f, hMean = 0.0f, hSd = 1.0f;
  if (haveBaseline) {
    stats(sTemp, n, tMean, tSd);
    stats(sHum, n, hMean, hSd);
  }

  const bool tValid = isfinite(sys.gTemp);
  const bool hValid = isfinite(sys.gHum);
  sys.anomalyTemp = (haveBaseline && tValid) ? (sys.gTemp - tMean) / tSd : 0.0f;
  sys.anomalyHum = (haveBaseline && hValid) ? (sys.gHum - hMean) / hSd : 0.0f;

  // Chỉ đưa mẫu hữu hạn vào cửa sổ: một NaN sẽ đầu độc mean/sd của cả 10 mẫu.
  if (tValid) sTemp[sIdx] = sys.gTemp;
  if (hValid) sHum[sIdx] = sys.gHum;
  sIdx = (sIdx + 1) % N;
  if (sIdx == 0) sFilled = true;

  sys.sensorHealthyTemp = tValid && sys.gTemp >= -10.0f && sys.gTemp <= 80.0f && fabsf(sys.anomalyTemp) < 4.0f;
  sys.sensorHealthyHum = hValid && sys.gHum >= 0.0f && sys.gHum <= 100.0f && fabsf(sys.anomalyHum) < 4.0f;
  // Không ghi đè sys.sensorHealthy: cờ đó do driver DHT quản lý (failCount).
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
