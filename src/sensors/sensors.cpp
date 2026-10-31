#include "../globals.h"
#include "../logger.h"
#include "../sensors.h"
#include "../hal.h"
#include "../calibration.h"

// ── Bộ đệm trung bình động ──
static float sTempBuf[SENSOR_FILTER_SIZE] = {0};
static float sHumBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sLightBuf[SENSOR_FILTER_SIZE] = {0};
static int   sSoilBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sWaterBuf[SENSOR_FILTER_SIZE] = {0};
static uint8_t sBufIdx = 0;
static bool sBufFilled = false;

// DHT dùng vòng RIÊNG và chỉ tiến khi có mẫu hợp lệ. Lý do: một lần đọc hỏng
// trước đây được "vá" bằng giá trị cũ (hoặc 0 nếu cửa sổ còn trống) rồi đẩy vào
// cùng một cửa sổ, nên NaN đầu tiên sau khi cấp điện nhét số 0 vào bộ lọc và nó
// nằm đó nhiều giây — sys.gTemp treo ở 0 °C trong khi mọi thứ phía sau (quạt,
// OLED, /api, weatherStatus) coi là số đo thật. Giữ mẫu cũ = không viết gì, để
// sys.gTemp/gHum đứng nguyên ở giá trị trung bình tốt cuối cùng.
static uint8_t sThIdx = 0;
static uint8_t sThCount = 0;      // số mẫu DHT thật đang có trong cửa sổ

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

// Kẹp kết quả sau hiệu chuẩn về miền ADC [0,4095]. Offset hiệu chuẩn có thể làm
// raw + offset vượt rail: gSoil ÂM thì "gSoil < soilThresholdLow" đúng vĩnh viễn
// (bơm chạy liên tục), tràn dương thì bơm không bao giờ chạy. Kẹp một lần nữa ở
// phía calibration.cpp là chủ ý — hai tầng độc lập, mỗi tầng tự đủ để an toàn.
static int clampAdc(float value) {
  if (!(value > 0.0f)) return 0;          // cả NaN/Inf rơi vào đây
  if (value > 4095.0f) return 4095;
  return (int)value;
}

int   readLight()       { return clampAdc(calibrationApplyLight(normalizeAdc(halAnalogRead(LDR_PIN), INVERT_LIGHT))); }
int   readSoil()        { return clampAdc(calibrationApplySoil(normalizeAdc(halAnalogRead(SOIL_PIN), INVERT_SOIL))); }
int   readWater()       { return clampAdc(calibrationApplyWater(normalizeAdc(halAnalogRead(WATER_PIN), INVERT_WATER))); }
bool  readTouch()       { return halDigitalRead(TOUCH_PIN) == HIGH; }
bool  readPIR()         { return halDigitalRead(PIR_PIN) == HIGH; }

void readAllSensors() {
  float t = readTemperature();
  float h = readHumidity();

  // Kiểm tra NaN TRƯỚC khi thay bằng giá trị cũ
  const bool tIsNan = isnan(t);
  const bool hIsNan = isnan(h);
  const bool dhtGood = !tIsNan && !hIsNan;

  if (!dhtGood) {
    if (++sys.sensorFailCount > 5 && sys.sensorHealthy) {
      sys.sensorHealthy = false;
      LOG_ERROR_CODE(501, "DHT11 sensor failing repeatedly");
    }
  } else {
    // Reset số lần lỗi khi thành công (dựa trên NaN gốc của cả hai đại lượng)
    sys.sensorFailCount = 0;
    sys.sensorHealthy = true;
  }

  int l = readLight();
  int s = readSoil();
  int w = readWater();

  // Đẩy vào bộ đệm vòng ADC, cập nhật tổng chạy: trừ giá trị cũ ở slot đang ghi
  // (0 nếu chưa đầy) rồi cộng giá trị mới — trung bình O(1).
  sLightSum -= sLightBuf[sBufIdx];
  sSoilSum  -= sSoilBuf[sBufIdx];
  sWaterSum -= sWaterBuf[sBufIdx];

  sLightBuf[sBufIdx] = l;
  sSoilBuf[sBufIdx]  = s;
  sWaterBuf[sBufIdx] = w;

  sLightSum += l;
  sSoilSum  += s;
  sWaterSum += w;

  sBufIdx = (sBufIdx + 1) % SENSOR_FILTER_SIZE;
  if (sBufIdx == 0) sBufFilled = true;

  uint8_t n = sBufFilled ? SENSOR_FILTER_SIZE : sBufIdx;
  if (n == 0) n = 1;

  if (dhtGood) {
    // Slot chưa từng ghi (count < size) mang giá trị 0 khởi tạo: không trừ gì cả.
    if (sThCount < SENSOR_FILTER_SIZE) sThCount++;
    else { sTempSum -= sTempBuf[sThIdx]; sHumSum -= sHumBuf[sThIdx]; }
    sTempBuf[sThIdx] = t;
    sHumBuf[sThIdx]  = h;
    sTempSum += t;
    sHumSum  += h;
    sThIdx = (sThIdx + 1) % SENSOR_FILTER_SIZE;
    sys.gTemp = sTempSum / sThCount;
    sys.gHum  = sHumSum / sThCount;
  }
  // Mẫu hỏng: sys.gTemp/sys.gHum GIỮ NGUYÊN trung bình tốt cuối cùng, không
  // viết thêm ước lượng bịa vào cửa sổ.

  sys.gLight = (int)(sLightSum / n);
  sys.gSoil  = (int)(sSoilSum / n);
  sys.gWater = (int)(sWaterSum / n);
  sys.gPir   = readPIR();
  sys.gTouch = readTouch();
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
  sThIdx = 0;
  sThCount = 0;
  sTempSum = sHumSum = 0.0f;
  sLightSum = sSoilSum = sWaterSum = 0;
}
#endif
