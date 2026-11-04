#include "../core/globals.h"
#include "../core/logger.h"
#include "sensors.h"
#include "../hal/hal.h"
#include "../core/rtos.h"
#include "../control/calibration.h"

// ── Bộ đệm trung bình động ──
static float sTempBuf[SENSOR_FILTER_SIZE] = {0};
static float sHumBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sLightBuf[SENSOR_FILTER_SIZE] = {0};
static int   sSoilBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sWaterBuf[SENSOR_FILTER_SIZE] = {0};
static uint8_t sBufIdx = 0;
static bool sBufFilled = false;

// Tổng chạy (running sum) để trung bình O(1) thay vì quét lại cửa sổ mỗi lần đọc.
static float sTempSum = 0.0f, sHumSum = 0.0f;
static long  sLightSum = 0, sSoilSum = 0, sWaterSum = 0;

float readTemperature() { return calibrationApplyTemp(dht.readTemperature()); }
float readHumidity()    { return calibrationApplyHum(dht.readHumidity()); }

// Áp dụng các cờ INVERT_* từ config.h để hướng đã tài liệu hóa luôn
// đúng: LDR sáng = cao, đất khô = thấp, nước đầy = cao.
static int normalizeAdc(int value, bool invert) {
  return invert ? (4095 - value) : value;
}

int   readLight()       { return (int)calibrationApplyLight(normalizeAdc(halAnalogRead(LDR_PIN), INVERT_LIGHT)); }
int   readSoil()        { return (int)calibrationApplySoil(normalizeAdc(halAnalogRead(SOIL_PIN), INVERT_SOIL)); }
int   readWater()       { return (int)calibrationApplyWater(normalizeAdc(halAnalogRead(WATER_PIN), INVERT_WATER)); }
bool  readTouch()       { return halDigitalRead(TOUCH_PIN) == HIGH; }
bool  readPIR()         { return halDigitalRead(PIR_PIN) == HIGH; }

void readAllSensors() {
  rtosLock();
  float t = readTemperature();
  float h = readHumidity();

  // Kiểm tra NaN TRƯỚC khi thay bằng giá trị cũ
  bool tIsNan = isnan(t);
  bool hIsNan = isnan(h);

  // Xử lý NaN từ DHT: thay bằng giá trị tốt cuối cùng đã biết
  // và đánh dấu kênh temp/hum unhealthy tới khi có lần đọc tốt liên tiếp.
  if (tIsNan || hIsNan) {
    if (tIsNan) {
      t = sBufFilled ? sTempBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0.0f;
    }
    if (hIsNan) {
      h = sBufFilled ? sHumBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0.0f;
    }
    if (++sys.sensorFailCount > 3) {
      if (tIsNan && sys.sensorHealthyTemp) {
        sys.sensorHealthyTemp = false;
        LOG_ERROR_CODE(501, "DHT11 temperature failing repeatedly");
      }
      if (hIsNan && sys.sensorHealthyHum) {
        sys.sensorHealthyHum = false;
        LOG_ERROR_CODE(501, "DHT11 humidity failing repeatedly");
      }
    }
  } else {
    // Chỉ reset khi CẢ HAI kênh đều đọc tốt — tránh một kênh NaN
    // bị "che" bởi kênh còn lại và làm mất đánh dấu lỗi.
    sys.sensorFailCount = 0;
    sys.sensorHealthyTemp = true;
    sys.sensorHealthyHum = true;
  }

  int rawL = halAnalogRead(LDR_PIN);
  int rawS = halAnalogRead(SOIL_PIN);
  int rawW = halAnalogRead(WATER_PIN);
  auto adcValid = [](int raw) { return raw >= 0 && raw <= 4095; };
  // Light
  if (!adcValid(rawL)) {
    if (++sys.lightFailCount > 3 && sys.lightHealthy) {
      sys.lightHealthy = false;
      LOG_ERROR_CODE(502, "LDR sensor failing repeatedly");
    }
  } else {
    sys.lightFailCount = 0;
    sys.lightHealthy = true;
  }
  if (!adcValid(rawS)) {
    if (++sys.soilFailCount > 3 && sys.soilHealthy) {
      sys.soilHealthy = false;
      LOG_ERROR_CODE(503, "Soil sensor failing repeatedly");
    }
  } else {
    sys.soilFailCount = 0;
    sys.soilHealthy = true;
  }
  if (!adcValid(rawW)) {
    if (++sys.waterFailCount > 3 && sys.waterHealthy) {
      sys.waterHealthy = false;
      LOG_ERROR_CODE(504, "Water sensor failing repeatedly");
    }
  } else {
    sys.waterFailCount = 0;
    sys.waterHealthy = true;
  }

  int l = readLight();
  int s = readSoil();
  int w = readWater();

  // Đẩy vào bộ đệm vòng, cập nhật tổng chạy: trừ giá trị cũ ở slot đang ghi
  // (0 nếu chưa đầy) rồi cộng giá trị mới — trung bình O(1).
  sTempSum  -= sTempBuf[sBufIdx];
  sHumSum   -= sHumBuf[sBufIdx];
  sLightSum -= sLightBuf[sBufIdx];
  sSoilSum  -= sSoilBuf[sBufIdx];
  sWaterSum -= sWaterBuf[sBufIdx];

  sTempBuf[sBufIdx]  = t;
  sHumBuf[sBufIdx]   = h;
  sLightBuf[sBufIdx] = l;
  sSoilBuf[sBufIdx]  = s;
  sWaterBuf[sBufIdx] = w;

  sTempSum  += t;
  sHumSum   += h;
  sLightSum += l;
  sSoilSum  += s;
  sWaterSum += w;

  sBufIdx = (sBufIdx + 1) % SENSOR_FILTER_SIZE;
  if (sBufIdx == 0) sBufFilled = true;

  uint8_t n = sBufFilled ? SENSOR_FILTER_SIZE : sBufIdx;
  if (n == 0) n = 1;

  sys.gTemp  = sTempSum / n;
  sys.gHum   = sHumSum / n;
  sys.gLight = (int)(sLightSum / n);
  sys.gSoil  = (int)(sSoilSum / n);
  sys.gWater = (int)(sWaterSum / n);
  sys.gPir   = readPIR();
  sys.gTouch = readTouch();
  // Health tổng là AND của mọi kênh: 1 kênh lỗi -> degraded, nhưng
  // control vẫn có thể chạy kênh khác tốt (xem control.cpp dùng per-channel).
  sys.sensorHealthy = sys.sensorHealthyTemp && sys.sensorHealthyHum &&
                      sys.lightHealthy && sys.soilHealthy && sys.waterHealthy;
  rtosUnlock();
}

// ── Đoán thời tiết đơn giản từ ánh sáng + độ ẩm (bài 11) ──
Weather weatherStatus() {
  bool bright = sys.gLight > (sys.lightThreshold + 50);
  bool dark = sys.gLight < (sys.lightThreshold - 50);
  bool humid = sys.gHum > 80.0f;
  if (bright) return humid ? WEATHER_CLOUDY : WEATHER_SUNNY;
  if (dark)   return humid ? WEATHER_RAIN : WEATHER_CLOUDY;
  return WEATHER_NORMAL;
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: xoá cửa sổ lọc trung bình về trạng thái vừa cấp điện.
void testResetSensorStatics() {
  for (uint8_t i = 0; i < SENSOR_FILTER_SIZE; i++) {
    sTempBuf[i] = sHumBuf[i] = 0;
    sLightBuf[i] = sSoilBuf[i] = sWaterBuf[i] = 0;
  }
  sBufIdx = 0;
  sBufFilled = false;
  sTempSum = sHumSum = 0.0f;
  sLightSum = sSoilSum = sWaterSum = 0;
}
#endif
