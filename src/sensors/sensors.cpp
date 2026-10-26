#include "../globals.h"
#include "../logger.h"
#include "../sensors.h"
#include "../hal.h"
#include "../rtos.h"
#include "../calibration.h"

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
  if (tIsNan) {
    t = sBufFilled ? sTempBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
    if (++sys.sensorFailCount > 5 && sys.sensorHealthy) {
      sys.sensorHealthy = false;
      LOG_ERROR_CODE(501, "DHT11 sensor failing repeatedly");
    }
  }
  if (hIsNan) {
    h = sBufFilled ? sHumBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
  }

  // Reset số lần lỗi khi thành công (dùng kiểm tra NaN gốc, không dùng giá trị thay thế)
  if (!tIsNan && !hIsNan) {
    sys.sensorFailCount = 0;
    sys.sensorHealthy = true;
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
