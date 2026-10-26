// Bộ test chạy trên host chuyên sâu cho firmware Smart Farm QE002.
//
// Bộ test biên dịch firmware THẬT (SmartFarm.ino + src/*.cpp) dựa trên các
// mock phần cứng điều khiển được trong test/mocks/ và thực hiện:
//   - unit test cho từng module logic điều khiển
//   - integration test qua setup()/loop() thật và các HTTP handler
//   - deep test: JSON contract nghiêm ngặt, giá trị biên, millis() wrap,
//     an toàn giữa bước của demo, bộ đếm throttle WiFi, hành vi debug-mode,
//     lưu settings bền qua một chu kỳ nguồn mô phỏng, và một lượt
//     fuzz/soak có seed kiểm tra các safety invariant sau mỗi loop().
//
// Các test gọi runAutomation() — chính hàm firmware mà loop() dùng — nên
// không có bản sao logic điều khiển nào trong test. Mỗi test được chạy qua
// RUN_TEST() để in dòng PASS/FAIL kèm số check của riêng nó.
//
// Biến môi trường:
//   FUZZ_ITERS=N   đổi số vòng fuzz mỗi seed (mặc định 30000)
//
// Cách build & chạy:  ./test/run_tests.sh   (xem test/README.md)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <random>
#include <type_traits>

#include "mock.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"
#include "control.h"
#include "demo.h"
#include "settings.h"
#include "oled.h"
#include "config.h"
#include "wifi.h"
#include "logger.h"
#include "ArduinoOTA.h"

// Các entry point thật của firmware (SmartFarm.ino)
void setup();
void loop();
void runAutomation();   // logic automation thật, loop() cũng gọi hàm này

static int gChecks = 0, gFails = 0;
static const char *gCurrentTest = "init";

// CHECK_EQ chỉ dành cho số nguyên/enum: dùng long long (không phải long) và
// chặn ngay lúc biên dịch nếu ai đó lỡ truyền số thực — phải dùng CHECK_NEAR.
template <typename A, typename B>
static void checkEq(A a, B b, int line, const char *sa, const char *sb) {
  static_assert((std::is_integral<A>::value || std::is_enum<A>::value) &&
                    (std::is_integral<B>::value || std::is_enum<B>::value),
                "CHECK_EQ chi dung cho so nguyen/kieu enum; so thuc phai dung CHECK_NEAR");
  gChecks++;
  if (static_cast<long long>(a) != static_cast<long long>(b)) {
    gFails++;
    printf("  FAIL [%s] line %d: %s (%lld) != %s (%lld)\n", gCurrentTest, line, sa,
           static_cast<long long>(a), sb, static_cast<long long>(b));
  }
}
#define CHECK_EQ(a, b) checkEq((a), (b), __LINE__, #a, #b)

#define CHECK(cond) do { \
  gChecks++; \
  if (!(cond)) { gFails++; printf("  FAIL [%s] line %d: %s\n", gCurrentTest, __LINE__, #cond); } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
  gChecks++; \
  if (fabs((double)(a) - (double)(b)) > (eps)) { \
    gFails++; printf("  FAIL [%s] line %d: %s (%.2f) != %s (%.2f)\n", \
                     gCurrentTest, __LINE__, #a, (double)(a), #b, (double)(b)); \
  } \
} while (0)

#define CHECK_STR(a, b) do { \
  gChecks++; \
  if (strcmp((a), (b)) != 0) { \
    gFails++; printf("  FAIL [%s] line %d: \"%s\" != \"%s\"\n", gCurrentTest, __LINE__, (a), (b)); \
  } \
} while (0)

// Chạy một test và in kết quả riêng: giúp khoanh vùng lỗi ngay trên output.
static void runTest(const char *name, void (*fn)()) {
  gCurrentTest = name;
  int checks0 = gChecks, fails0 = gFails;
  fn();
  printf("[%s] %s (%d checks, %d fail)\n", gFails == fails0 ? "PASS" : "FAIL",
         name, gChecks - checks0, gFails - fails0);
}
#define RUN_TEST(fn) runTest(#fn, fn)

// ==============================================
//  Các hàm phụ trợ
// ==============================================

static bool has(const char *hay, const char *needle) {
  return hay && needle && strstr(hay, needle) != nullptr;
}

static int countOccurrences(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  int n = 0;
  for (const char *p = hay; (p = strstr(p, needle)) != nullptr; p += strlen(needle)) n++;
  return n;
}

static uint32_t currentLedPwm() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return mockPwm[BLUE_LED_PIN];
#else
  return mockPwm[LED_PWM_CHANNEL];
#endif
}

static void checkRelayPin(bool on) {
#if RELAY_ACTIVE_LOW
  CHECK_EQ(mockPinLevel[RELAY_PIN], on ? LOW : HIGH);
#else
  CHECK_EQ(mockPinLevel[RELAY_PIN], on ? HIGH : LOW);
#endif
}

static void checkBuzzer(bool on) {
#if BUZZER_PASSIVE
  if (on) CHECK(mockBuzzerToneHz > 0);
  else    CHECK_EQ(mockBuzzerToneHz, 0);
#else
  CHECK_EQ(mockPinLevel[BUZZER_PIN], on ? HIGH : LOW);
#endif
}

#ifdef SMARTFARM_TEST_HOOKS
// Hook do firmware cung cấp (chỉ tồn tại ở bản build test): trả các biến
// static trong src/ về trạng thái vừa cấp điện → test độc lập thứ tự hơn.
void testResetActuatorStatics();
void testResetControlStatics();
void testResetDemoStatics();
void testResetSensorStatics();
void testResetSettingsStatics();
void testResetWifiStatics();
void testResetLoggerStatics();
void testResetOledStatics();
// Hook từ src/webserver.cpp: dựng JSON /api vào buffer cho trước để kiểm
// tra nhánh "JSON bị cắt cụt".
int testBuildApiJson(char* buf, size_t cap);
#endif

static void resetState() {
  mockReset();
#ifdef SMARTFARM_TEST_HOOKS
  testResetActuatorStatics();
  testResetControlStatics();
  testResetDemoStatics();
  testResetSensorStatics();
  testResetSettingsStatics();
  testResetWifiStatics();
  testResetLoggerStatics();
  testResetOledStatics();
#endif
  gMinLogLevel = LOG_INFO;
  // Ngưỡng + cờ auto lấy từ chính firmware (một nguồn sự thật, không chép
  // lại hằng số mặc định ở đây); resetSettings() cũng xóa flash như mockReset().
  resetSettings();
  sys.relayState = sys.fanState = sys.blueLedState = sys.breatheLedState = false;
  sys.windmillOn = false;
  sys.rgbR = sys.rgbG = sys.rgbB = 0;
  sys.breatheBrightness = 0;
  sys.breatheDirection = 1;
  sys.servoAngle = 0;
  sys.gTemp = 22; sys.gHum = 50; sys.gLight = 300; sys.gSoil = 500; sys.gWater = 800;
  sys.gPir = false; sys.gTouch = false;
  sys.lastWifiCheck = sys.lastSensorRead = sys.lastOledUpdate = 0;
  sys.lastBuzzer = sys.lastTouch = sys.lastBreatheUpdate = 0;
  sys.sensorFailCount = 0;
  sys.sensorHealthy = true;
}

static int invoke(const char *path,
                  const char *a1 = nullptr, const char *v1 = nullptr,
                  const char *a2 = nullptr, const char *v2 = nullptr,
                  const char *a3 = nullptr, const char *v3 = nullptr) {
  if (WEB_TOKEN[0] != '\0') mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
  if (a1) mockServerSetArg(a1, v1);
  if (a2) mockServerSetArg(a2, v2);
  if (a3) mockServerSetArg(a3, v3);
  bool found = mockServerInvoke(path);
  CHECK(found);
  return mockServerLastCode();
}

static void runLoop(unsigned long simMs, unsigned long stepMs = 10) {
  unsigned long start = mockMillis();
  while (mockMillis() - start < simMs) {
    mockAdvance(stepMs);
    loop();
  }
}

static void primeSensors(int water) {
  mockDhtTemp = 26.0f; mockDhtHum = 60.0f;
  mockAdc[LDR_PIN] = 1000; mockAdc[SOIL_PIN] = 600;
  mockAdc[WATER_PIN] = INVERT_WATER ? 4095 - water : water;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  sys.lastSensorRead = mockMillis();
}

// Bản build "tinyjson" dùng buffer dựng JSON /api cố ý nhỏ (64 byte) để kiểm
// tra nhánh "JSON bị cắt cụt" — khi đó /api phải trả lỗi 500 thay vì JSON
// hỏng, nên các kiểm tra nội dung JSON không áp dụng cho bản đó.
#if API_JSON_BUF_SIZE < 512
#define API_JSON_IS_TINY 1
#else
#define API_JSON_IS_TINY 0
#endif

// Danh sách route là NGUỒN DUY NHẤT: thêm route mới chỉ cần sửa một chỗ,
// không còn con số 15 lặp lại rải rác trong các test.
static const char *ALL_ROUTES[] = {
  "/", "/api", "/toggle", "/auto", "/rgb", "/servo", "/breathe", "/on", "/off",
  "/windmill", "/feed", "/demo", "/threshold", "/wifi", "/favicon.ico", "/health", "/calibrate", "/metrics"
};
static const int ALL_ROUTE_COUNT = (int)(sizeof(ALL_ROUTES) / sizeof(ALL_ROUTES[0]));

static void checkAllRoutes() {
  CHECK_EQ(mockServerRouteCount(), ALL_ROUTE_COUNT);
  for (const char *r : ALL_ROUTES) CHECK(mockServerHasRoute(r));
}

// Số vòng fuzz mỗi seed; có thể rút ngắn khi phát triển bằng FUZZ_ITERS.
static int fuzzIterations() {
  const char *env = getenv("FUZZ_ITERS");
  if (env && *env) {
    int v = atoi(env);
    if (v > 0) return v;
  }
  return 30000;
}

// ==============================================
//  Unit test
// ==============================================

// T1: trung bình cảm biến + hướng đảo tín hiệu
static void testSensorsAverage() {
  printf("T1  sensors: moving average + direction flags\n");
  resetState();
  mockDhtTemp = 26.5f; mockDhtHum = 61.0f;
  mockAdc[LDR_PIN] = 3000; mockAdc[SOIL_PIN] = 600; mockAdc[WATER_PIN] = 900;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();

  auto dir = [](int raw, int invert) { return invert ? 4095 - raw : raw; };
  CHECK_EQ(sys.gLight, dir(3000, INVERT_LIGHT));
  CHECK_EQ(sys.gSoil, dir(600, INVERT_SOIL));
  CHECK_EQ(sys.gWater, dir(900, INVERT_WATER));
  CHECK_NEAR(sys.gTemp, 26.5, 0.05);
  CHECK_NEAR(sys.gHum, 61.0, 0.05);

  mockAdc[LDR_PIN] = 2000;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_EQ(sys.gLight, dir(2000, INVERT_LIGHT));

  mockAdc[LDR_PIN] = 1000;
  readAllSensors();
  int expected = (4 * dir(2000, INVERT_LIGHT) + dir(1000, INVERT_LIGHT)) / 5;
  CHECK_EQ(sys.gLight, expected);
}

// T2: DHT NaN failover + cờ health
static void testDhtFailover() {
  printf("T2  sensors: DHT11 NaN failover\n");
  resetState();
  mockDhtTemp = 26.0f; mockDhtHum = 55.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_NEAR(sys.gTemp, 26.0, 0.05);

  mockDhtTemp = NAN;
  readAllSensors();
  CHECK_NEAR(sys.gTemp, 26.0, 0.05);
  for (int i = 0; i < 6; i++) readAllSensors();
  CHECK(!sys.sensorHealthy);

  mockDhtTemp = 24.0f; mockDhtHum = 50.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK(sys.sensorHealthy);
  CHECK_NEAR(sys.gTemp, 24.0, 0.05);

  // Độ ẩm NaN dùng lại giá trị lọc tốt cuối cùng (giống nhiệt độ)
  mockDhtTemp = 27.0f; mockDhtHum = NAN;
  readAllSensors();
  CHECK_NEAR(sys.gHum, 50.0, 0.5);
  CHECK(sys.sensorHealthy);        // một NaN độ ẩm đơn lẻ không đánh dấu cảm biến lỗi
  mockDhtHum = 50.0f;
  readAllSensors();
}

// T3: trễ bật/tắt bơm theo độ ẩm đất (hoạt động 7/10)
static void testPumpHysteresis() {
  printf("T3  pump: soil hysteresis\n");
  resetState();
  sys.gWater = 900;
  sys.gSoil = 300;
  controlPump();
  CHECK(sys.relayState);
  checkRelayPin(true);
  sys.gSoil = 500;                 // giữa các ngưỡng: không đổi
  controlPump();
  CHECK(sys.relayState);
  sys.gSoil = 800;
  controlPump();
  CHECK(!sys.relayState);
  checkRelayPin(false);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  sys.soilThresholdLow = 700; sys.soilThresholdHigh = 400;
  sys.gSoil = 500; sys.gWater = 900;
  controlPump();
  CHECK(!sys.relayState);
  controlPump();
  CHECK(!sys.relayState);
}

// T4: an toàn hết nước của bơm kể cả ở chế độ thủ công (hoạt động 8)
static void testPumpLowWaterSafety() {
  printf("T4  pump: low-water safety\n");
  resetState();
  sys.pumpAuto = false;
  setRelay(true);
  CHECK(sys.relayState);
  sys.gWater = 100;
  controlPump();
  CHECK(!sys.relayState);

  sys.pumpAuto = true;
  sys.gSoil = 100;                 // rất khô nhưng không có nước
  sys.gWater = 100;
  controlPump();
  CHECK(!sys.relayState);
}

// T5: trễ bật/tắt quạt theo nhiệt độ
static void testFanHysteresis() {
  printf("T5  fan: temperature hysteresis\n");
  resetState();
  sys.gTemp = 31; controlFan(); CHECK(sys.fanState);
  sys.gTemp = 27; controlFan(); CHECK(sys.fanState);
  sys.gTemp = 24; controlFan(); CHECK(!sys.fanState);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  sys.tempThresholdHigh = 25; sys.tempThresholdLow = 30;
  sys.gTemp = 27;
  controlFan();
  CHECK(!sys.fanState);
  controlFan();
  CHECK(!sys.fanState);
}

// T6: đèn auto + PIR chiếm quyền + cảnh báo
static void testLightPirAlerts() {
  printf("T6  light auto / PIR / alerts\n");
  resetState();
  sys.gLight = 400;                // tối
  controlLight();
  CHECK(sys.blueLedState);
  sys.gLight = 600;                // sáng
  controlLight();
  CHECK(!sys.blueLedState);

  sys.gPir = true;                 // PIR nắm LED: đèn auto phải nhường
  sys.gLight = 400;
  controlLight();
  CHECK(!sys.blueLedState);

  controlAlerts();             // PIR bật đèn
  CHECK(sys.blueLedState);
  sys.gPir = false;
  controlAlerts();             // và tắt khi hết chuyển động
  CHECK(!sys.blueLedState);
}

// T7: LED xanh sáng đều vs breathing (hoạt động 3) + điều phối PWM
static void testBreatheAndLedModes() {
  printf("T7  LED: steady/breathe modes + PWM dispatch\n");
  resetState();
  setBreatheLed(true);
  CHECK(sys.breatheLedState);
  CHECK(!sys.blueLedState);
  CHECK_EQ(currentLedPwm(), 0);

  uint32_t first = 0, peak = 0;
  for (int i = 0; i < 400; i++) {
    mockAdvance(BREATHE_INTERVAL);
    controlBreathe();
    uint32_t p = currentLedPwm();
    if (p && !first) first = p;
    if (p > peak) peak = p;
  }
  CHECK_EQ(first, 5);
  CHECK_EQ(peak, 255);

  setBlueLed(true);            // lệnh sáng đều thắng breathing
  CHECK(!sys.breatheLedState);
  CHECK(sys.blueLedState);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);

  setLedPwm(123);
  CHECK_EQ(currentLedPwm(), 123);
}

// T8: chạm bật/tắt kèm debounce (hoạt động 4)
static void testTouchToggle() {
  printf("T8  touch: edge toggle + debounce\n");
  resetState();
  mockAdvance(5000);
  sys.gTouch = true;
  controlTouch();
  CHECK(sys.blueLedState);
  controlTouch();              // giữ: không lặp lại
  CHECK(sys.blueLedState);
  sys.gTouch = false;
  controlTouch();
  sys.gTouch = true;
  mockAdvance(TOUCH_DEBOUNCE - 100);
  controlTouch();              // trong thời gian debounce: lần nhấn bị nuốt
  CHECK(sys.blueLedState);
  sys.gTouch = false;
  controlTouch();              // nhả
  mockAdvance(200);            // lúc này đã qua cửa sổ debounce
  sys.gTouch = true;
  controlTouch();              // tắt
  CHECK(!sys.blueLedState);
  CHECK(!sys.lightAuto);           // thao tác thủ công đã tắt auto
}

// T9: đèn RGB trồng cây (hoạt động 9)
static void testRgbPlant() {
  printf("T9  RGB: plant pattern (activity 9)\n");
  resetState();
  setRgb(10, 20, 30);
  CHECK_EQ(mockPixel[3], (10 << 16) | (20 << 8) | 30);

  sys.gLight = 400;                // tối -> kiểu đèn cho cây
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
  CHECK_EQ(mockPixel[1], 0x0000FF);
  CHECK_EQ(mockPixel[2], 0xFF0000);
  CHECK_EQ(mockPixel[3], 0x0000FF);
  CHECK_EQ(sys.rgbR, 255); CHECK_EQ(sys.rgbG, 0); CHECK_EQ(sys.rgbB, 255);

  sys.gLight = 600;                // sáng -> tắt
  controlRgb();
  for (int i = 0; i < RGB_COUNT; i++) CHECK_EQ(mockPixel[i], 0);
  CHECK_EQ(sys.rgbR, 0); CHECK_EQ(sys.rgbB, 0);

  setRgb(10, 20, 30);          // màu thủ công, rồi tối lại
  sys.gLight = 400;
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);

  // Màu đơn (255,0,255) trùng đúng giá trị RGB của hoa văn: nếu suy đoán
  // "đã bật hoa văn" từ màu RGB thì đèn tăng trưởng sẽ không bật khi trời tối.
  // Phải theo cờ thật của dải LED mới đúng.
  resetState();
  setRgb(255, 0, 255);
  CHECK(!rgbPlantPatternActive());          // màu đơn ≠ hoa văn
  sys.gLight = 400;
  controlRgb();
  CHECK(rgbPlantPatternActive());           // hoa văn đã bật
  CHECK_EQ(mockPixel[0], 0xFF0000);
  CHECK_EQ(mockPixel[1], 0x0000FF);
  CHECK_EQ(mockPixel[2], 0xFF0000);
  CHECK_EQ(mockPixel[3], 0x0000FF);
}

// T10: còi báo động hết nước (hoạt động 8)
static void testBuzzerAlarm() {
  printf("T10 buzzer: low-water alarm\n");
  resetState();
  checkBuzzer(false);
  sys.gWater = 100;
  controlAlerts();
  checkBuzzer(true);
  sys.gWater = 800;
  controlAlerts();
  checkBuzzer(false);
}

// T11: servo cối xay gió quét (hoạt động 6)
static void testWindmillSweep() {
  printf("T11 windmill: servo sweep\n");
  resetState();
  setWindmill(true);
  CHECK(sys.windmillOn);
  int minA = 999, maxA = -1;
  for (int i = 0; i < 120; i++) {
    mockAdvance(WINDMILL_INTERVAL);
    controlWindmill();
    if (mockServoAngle < minA) minA = mockServoAngle;
    if (mockServoAngle > maxA) maxA = mockServoAngle;
  }
  CHECK(minA >= WINDMILL_MIN_ANGLE);
  CHECK(maxA <= WINDMILL_MAX_ANGLE);
  CHECK_EQ(maxA, WINDMILL_MAX_ANGLE);
  CHECK(minA < maxA);
  setWindmill(false);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
}

// T12: thời gian cửa cho ăn + phân xử servo dùng chung (hoạt động 10)
static void testFeedGate() {
  printf("T12 feed: gate timing + shared servo\n");
  resetState();
  startFeed();
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  mockAdvance(FEED_DURATION - 1);
  controlFeed();
  CHECK(feedRunning());
  mockAdvance(1);
  controlFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  setWindmill(true);
  startFeed();                 // cho ăn chiếm servo dùng chung
  CHECK(feedRunning());
  CHECK(!sys.windmillOn);
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  setWindmill(true);           // cối xay gió lấy lại
  CHECK(!feedRunning());
  CHECK(sys.windmillOn);
  setWindmill(false);

  // stopFeed() phải ĐÓNG cửa máng ăn: servo dùng chung với cối xay gió,
  // nếu chỉ ngừng mà không đóng lại thì cửa bị kẹt mở khi servo chạy sang
  // mục khác.
  startFeed();
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  stopFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  // stopFeed() khi không có cho ăn thì không được đụng servo
  mockServoWriteCount = 0;
  stopFeed();
  CHECK_EQ(mockServoWriteCount, 0);
}

// T13: đoán thời tiết (hoạt động 11)
static void testWeather() {
  printf("T13 weather: light + humidity guess\n");
  resetState();
  sys.gLight = 800; sys.gHum = 50; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  sys.gHum = 90;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  sys.gLight = 200; sys.gHum = 90; CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  sys.gHum = 50;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  sys.gLight = 500;            CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
}

// T14: lưu/đọc settings vòng tròn
static void testSettingsRoundtrip() {
  printf("T14 settings: save/load roundtrip\n");
  resetState();
  sys.soilThresholdLow = 111; sys.soilThresholdHigh = 222; sys.waterThresholdLow = 133;
  sys.lightThreshold = 654; sys.tempThresholdHigh = 35.5f; sys.tempThresholdLow = 21.5f;
  sys.pumpAuto = false; sys.lightAuto = false; sys.rgbAuto = true; sys.fanAuto = false; sys.breatheAuto = true;
  saveSettings();

  sys.soilThresholdLow = 0; sys.soilThresholdHigh = 0; sys.waterThresholdLow = 0;
  sys.lightThreshold = 0; sys.tempThresholdHigh = 0; sys.tempThresholdLow = 0;
  sys.pumpAuto = true; sys.lightAuto = true; sys.rgbAuto = false; sys.fanAuto = true; sys.breatheAuto = false;
  loadSettings();

  CHECK_NEAR(sys.soilThresholdLow, 111, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 222, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 133, 0.01);
  CHECK_EQ(sys.lightThreshold, 654);
  CHECK_NEAR(sys.tempThresholdHigh, 35.5, 0.01);
  CHECK_NEAR(sys.tempThresholdLow, 21.5, 0.01);
  CHECK(!sys.pumpAuto); CHECK(!sys.lightAuto); CHECK(sys.rgbAuto); CHECK(!sys.fanAuto); CHECK(sys.breatheAuto);
}

// T18: resetSettings xóa flash + bỏ qua commit khi giá trị không đổi
static void testSettingsResetAndSkip() {
  printf("T18 settings: reset defaults + skip unchanged writes\n");
  resetState();

  // Lưu cấu hình khác mặc định
  sys.soilThresholdLow = 321; sys.pumpAuto = false;
  saveSettings();
  CHECK_NEAR(mockPrefGet("soilLow", 0), 321, 0.01);
  CHECK_NEAR(mockPrefGet("pumpAuto", 1), 0, 0.01);

  // Mã bảo mật KHÔNG được ghi vào flash dạng plaintext
  strncpy(sys.otaPassword, "ota-test-123", sizeof(sys.otaPassword) - 1);
  sys.otaPassword[sizeof(sys.otaPassword) - 1] = '\0';
  strncpy(sys.webToken, "token-test-456", sizeof(sys.webToken) - 1);
  sys.webToken[sizeof(sys.webToken) - 1] = '\0';
  saveSettings();
  CHECK_STR(mockPrefGetStr("otaPass", ""), "");
  CHECK_STR(mockPrefGetStr("webToken", ""), "");

  // Lưu lần hai khi mọi giá trị không đổi: bỏ qua, không commit flash
  gMinLogLevel = LOG_DEBUG;
  mockSerialClear();
  saveSettings();
  CHECK(has(mockSerialOutput(), "Settings unchanged"));
  gMinLogLevel = LOG_INFO;

  // Đổi một giá trị: khóa đó được ghi lại
  sys.soilThresholdLow = 322;
  saveSettings();
  CHECK_NEAR(mockPrefGet("soilLow", 0), 322, 0.01);

  // loadSettings không đụng tới mã bảo mật trong RAM
  sys.otaPassword[0] = '\0';
  loadSettings();
  CHECK_STR(sys.otaPassword, "");

  // resetSettings: flash sạch, mọi thứ về mặc định
  resetSettings();
  CHECK_NEAR(sys.soilThresholdLow, 400, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 700, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 200, 0.01);
  CHECK_EQ(sys.lightThreshold, 500);
  CHECK_NEAR(sys.tempThresholdHigh, 30, 0.01);
  CHECK_NEAR(sys.tempThresholdLow, 25, 0.01);
  CHECK(sys.pumpAuto); CHECK(sys.lightAuto); CHECK(sys.rgbAuto); CHECK(sys.fanAuto);
  CHECK(!sys.breatheAuto);
  CHECK_STR(sys.ssid, "Tab S10 Lite của Trinh");
  CHECK_STR(sys.webToken, WEB_TOKEN);
  CHECK_STR(sys.otaPassword, OTA_PASSWORD);
  CHECK(!sys.gNeedRestart);
  CHECK_NEAR(mockPrefGet("soilLow", -1), -1, 0.01);      // flash đã bị xóa

  // Lần lưu sau reset phải ghi lại toàn bộ (bản chụp đã bị vô hiệu)
  saveSettings();
  CHECK_NEAR(mockPrefGet("soilLow", -1), 400, 0.01);

  resetState();
}

// T19: logger an toàn với cấp log ngoài enum
static void testLoggerLevelGuard() {
  printf("T19 logger: invalid level label\n");
  mockSerialClear();
  logMessage((LogLevel)99, "test", 1, "bad level");
  CHECK(has(mockSerialOutput(), "?????"));
  CHECK(has(mockSerialOutput(), "bad level"));
}

// T20: giá trị NVS không hữu hạn (flash hỏng) quay về mặc định
static void testSettingsNonFiniteFallback() {
  printf("T20 settings: non-finite NVS floats fall back to defaults\n");
  resetState();
  mockPrefPut("soilLow", NAN);
  mockPrefPut("soilHigh", INFINITY);
  mockPrefPut("waterLow", -INFINITY);
  mockPrefPut("tempHigh", NAN);
  mockPrefPut("tempLow", INFINITY);
  mockPrefPut("lightThr", 123);

  loadSettings();
  CHECK(isfinite(sys.soilThresholdLow));       // không được giữ NaN/Inf
  CHECK(isfinite(sys.soilThresholdHigh));
  CHECK(isfinite(sys.waterThresholdLow));
  CHECK(isfinite(sys.tempThresholdHigh));
  CHECK(isfinite(sys.tempThresholdLow));
  CHECK_NEAR(sys.soilThresholdLow, 400, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 700, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 200, 0.01);
  CHECK_NEAR(sys.tempThresholdHigh, 30, 0.01);
  CHECK_NEAR(sys.tempThresholdLow, 25, 0.01);
  CHECK_EQ(sys.lightThreshold, 123);           // giá trị hữu hạn vẫn được giữ

  // Điều khiển phải chạy lại bình thường với các giá trị đã sửa
  sys.gWater = 900; sys.gSoil = 100;
  controlPump();
  CHECK(sys.relayState);
  setRelay(false);
  resetState();
}

// T15: chạy demo đầy đủ (hoạt động 2-12)
static void testDemoFullRun() {
  printf("T15 demo: full sequence\n");
  resetState();
  primeSensors(900);
  CHECK(sys.gWater > (int)sys.waterThresholdLow);

  startDemo();
  CHECK(demoRunning());
  bool sawRelay = false, inBreatheStep = false;
  uint32_t breatheMin = 9999, breatheMax = 0;
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    runAutomation();
    if (sys.relayState) sawRelay = true;
    if (strstr(demoStepName(), "Den tho")) {
      inBreatheStep = true;
      uint32_t p = currentLedPwm();
      if (p > breatheMax) breatheMax = p;
      if (p < breatheMin) breatheMin = p;
    }
  }
  CHECK(!demoRunning());
  CHECK(sawRelay);                       // HD7 bơm khi có nước
  CHECK(inBreatheStep);                  // bước HD3 đã chạy
  CHECK(breatheMax > 0);                 // và LED thực sự breathing
  CHECK(breatheMin < breatheMax);
  CHECK(!sys.relayState);
  CHECK(!sys.blueLedState);
  CHECK(!sys.breatheLedState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);
  CHECK_EQ(mockPixel[0], 0);

  // Hết nước: bước bơm phải bị bỏ qua suốt lượt chạy
  resetState();
  primeSensors(100);
  CHECK(sys.gWater < (int)sys.waterThresholdLow);
  startDemo();
  sawRelay = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    runAutomation();
    if (sys.relayState) sawRelay = true;
  }
  CHECK(!demoRunning());
  CHECK(!sawRelay);
  checkBuzzer(false);
}

// D1: biên ngưỡng chính xác
static void testBoundaries() {
  printf("D1  boundary: exact threshold comparisons\n");

  // Bơm: so sánh nghiêm ngặt < / >
  resetState();
  sys.gWater = 900;
  sys.gSoil = 400; controlPump(); CHECK(!sys.relayState);            // == low: không khởi động
  sys.gSoil = 399; controlPump(); CHECK(sys.relayState);             // < low: khởi động
  sys.gSoil = 700; controlPump(); CHECK(sys.relayState);             // == high: vẫn bật
  sys.gSoil = 701; controlPump(); CHECK(!sys.relayState);            // > high: dừng
  sys.gWater = 200; sys.gSoil = 399; controlPump(); CHECK(sys.relayState);  // == waterLow: được phép
  sys.gWater = 199; controlPump(); CHECK(!sys.relayState);           // < waterLow: bị chặn

  // Quạt: nghiêm ngặt > / <
  resetState();
  sys.gTemp = 30;    controlFan(); CHECK(!sys.fanState);             // == high: không khởi động
  sys.gTemp = 30.01f; controlFan(); CHECK(sys.fanState);             // > high: khởi động
  sys.gTemp = 25;    controlFan(); CHECK(sys.fanState);              // == low: vẫn bật
  sys.gTemp = 24.99f; controlFan(); CHECK(!sys.fanState);            // < low: dừng

  // Đèn: ngưỡng +/- 50 với so sánh nghiêm ngặt
  resetState();
  sys.gLight = 450; controlLight(); CHECK(!sys.blueLedState);        // == t-50: không bật
  sys.gLight = 449; controlLight(); CHECK(sys.blueLedState);         // < t-50: bật
  sys.gLight = 550; controlLight(); CHECK(sys.blueLedState);         // == t+50: vẫn bật
  sys.gLight = 551; controlLight(); CHECK(!sys.blueLedState);        // > t+50: tắt

  // Thời tiết: cùng dải +/- 50 và độ ẩm > 80
  resetState();
  sys.gHum = 50;
  sys.gLight = 550; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  sys.gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  sys.gLight = 450; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  sys.gLight = 449; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  sys.gHum = 80;    CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);   // == 80 không tính là ẩm
  sys.gHum = 81;    CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  sys.gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  sys.gHum = 50.5f; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);

  // Còi: nghiêm ngặt < waterLow
  resetState();
  sys.gPir = false;
  sys.gWater = 200; controlAlerts(); checkBuzzer(false);
  sys.gWater = 199; controlAlerts(); checkBuzzer(true);
  sys.gWater = 200; controlAlerts(); checkBuzzer(false);
}

// D2: tính idempotent của actuator + nhất quán dây chân
static void testIdempotency() {
  printf("D2  actuators: idempotency + pin levels\n");
  resetState();
  setRelay(false);
  setFan(false);
  setBuzzer(false);
  setBlueLed(false);
  setBreatheLed(false);

  int d0 = mockDigitalWriteCount;
  setRelay(true);
  CHECK_EQ(mockDigitalWriteCount, d0 + 1);
  setRelay(true);                                // idempotent: không ghi lại
  CHECK_EQ(mockDigitalWriteCount, d0 + 1);
  checkRelayPin(true);
  setRelay(false);
  CHECK_EQ(mockDigitalWriteCount, d0 + 2);
  checkRelayPin(false);

  int d1 = mockDigitalWriteCount;
  setFan(true);
  CHECK_EQ(mockDigitalWriteCount, d1 + 1);
  setFan(true);
  CHECK_EQ(mockDigitalWriteCount, d1 + 1);
  CHECK_EQ(mockPinLevel[FAN_PIN], HIGH);
  setFan(false);
  CHECK_EQ(mockDigitalWriteCount, d1 + 2);
  CHECK_EQ(mockPinLevel[FAN_PIN], LOW);

#if BUZZER_PASSIVE
  int t0 = mockToneCalls, n0 = mockNoToneCalls;
  setBuzzer(true);
  setBuzzer(true);
  CHECK_EQ(mockToneCalls, t0 + 1);
  checkBuzzer(true);
  setBuzzer(false);
  CHECK_EQ(mockNoToneCalls, n0 + 1);
  checkBuzzer(false);
#else
  int d2 = mockDigitalWriteCount;
  setBuzzer(true);
  setBuzzer(true);
  CHECK_EQ(mockDigitalWriteCount, d2 + 1);
  checkBuzzer(true);
  setBuzzer(false);
  CHECK_EQ(mockDigitalWriteCount, d2 + 2);
  checkBuzzer(false);
#endif

  setBlueLed(true);
  int p0 = mockLedcWriteCount;
  setBlueLed(true);                              // idempotent: không ghi lại
  CHECK_EQ(mockLedcWriteCount, p0);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);
  setBreatheLed(false);
  int p1 = mockLedcWriteCount;
  setBreatheLed(false);                          // đã tắt: không làm gì
  CHECK_EQ(mockLedcWriteCount, p1);

  // NeoPixel: màu không đổi -> bỏ qua strip.show() (giao dịch RMT ngắt interrupts)
  int s0 = mockStripShowCount;
  setRgb(255, 0, 0);
  CHECK_EQ(mockStripShowCount, s0 + 1);
  setRgb(255, 0, 0);                             // idempotent: không show lại
  CHECK_EQ(mockStripShowCount, s0 + 1);
  setRgb(0, 0, 255);
  CHECK_EQ(mockStripShowCount, s0 + 2);
  setRgbPlant();                                 // hoa văn đỏ/xanh xen kẽ
  CHECK_EQ(mockStripShowCount, s0 + 3);
  setRgb(255, 0, 255);                           // hoa văn -> màu đơn sắc trùng giá trị: vẫn phải show
  CHECK_EQ(mockStripShowCount, s0 + 4);
  setRgb(255, 0, 255);                           // giờ mới thực sự idempotent
  CHECK_EQ(mockStripShowCount, s0 + 4);
  setRgbPlant();                                 // bật lại hoa văn luôn show
  CHECK_EQ(mockStripShowCount, s0 + 5);
}

// D3: biên thời gian chính xác + ghi servo tại các mốc biên
static void testTimingBoundaries() {
  printf("D3  timing: exact interval boundaries\n");

  // Debounce chạm: >= TOUCH_DEBOUNCE
  resetState();
  mockAdvance(10000);
  sys.gTouch = false; controlTouch();
  sys.lastTouch = mockMillis();
  setBlueLed(false);
  sys.gTouch = true; controlTouch();                  // delta 0: bị nuốt
  CHECK(!sys.blueLedState);
  sys.gTouch = false; controlTouch();
  mockAdvance(TOUCH_DEBOUNCE - 1);                // 299: bị nuốt
  sys.gTouch = true; controlTouch();
  CHECK(!sys.blueLedState);
  sys.gTouch = false; controlTouch();
  mockAdvance(1);                                 // đúng 300: được phép
  sys.gTouch = true; controlTouch();
  CHECK(sys.blueLedState);

  // Cửa cho ăn: đóng đúng tại FEED_DURATION
  resetState();
  startFeed();
  mockAdvance(FEED_DURATION - 1); controlFeed(); CHECK(feedRunning());
  mockAdvance(1);                 controlFeed(); CHECK(!feedRunning());

  // Breathing: cập nhật đầu tiên đúng tại BREATHE_INTERVAL
  resetState();
  setBreatheLed(true);
  setLedPwm(0);
  sys.lastBreatheUpdate = mockMillis();
  mockAdvance(BREATHE_INTERVAL - 1); controlBreathe(); CHECK_EQ(currentLedPwm(), 0);
  mockAdvance(1);                    controlBreathe(); CHECK_EQ(currentLedPwm(), 5);
  setBreatheLed(false);

  // Cối xay gió: bước quét chạy đúng tại WINDMILL_INTERVAL
  resetState();
  setWindmill(true);
  mockAdvance(1000);
  controlWindmill();                              // đồng bộ timer khoảng thời gian
  int c0 = mockServoWriteCount;
  mockAdvance(WINDMILL_INTERVAL - 1); controlWindmill(); CHECK_EQ(mockServoWriteCount, c0);
  mockAdvance(1);                     controlWindmill(); CHECK_EQ(mockServoWriteCount, c0 + 1);
  setWindmill(false);

  // Ghi servo tại đúng các mốc biên
  resetState();
  sys.servoAngle = 0;   servo.write(sys.servoAngle);   CHECK_EQ(mockServoAngle, 0);
  sys.servoAngle = 180; servo.write(sys.servoAngle);   CHECK_EQ(mockServoAngle, 180);
  sys.servoAngle = 0;   servo.write(sys.servoAngle);
}

// ==============================================
//  Integration test (setup/loop thật, HTTP, OLED, serial)
// ==============================================

// I1: khởi động firmware
static void testInoBoot() {
  printf("I1  boot: setup() registers routes\n");
  mockSerialClear();
  mockServerReset();
  setup();
  checkAllRoutes();     // gồm cả /wifi (trước đây bị bỏ sót trong danh sách)
  CHECK(has(mockSerialOutput(), "SMART FARM DEBUG MENU"));
  CHECK(has(mockSerialOutput(), "System ready"));
}

// I2: JSON contract của /api
static void testHttpApi() {
  printf("I2  http: /api JSON\n");
#if API_JSON_IS_TINY
  CHECK_EQ(invoke("/api"), 500);                 // JSON bị cắt -> báo lỗi
  CHECK(has(mockServerLastBody(), "too large"));
  return;
#endif
  sys.gTemp = 26.5f; sys.gHum = 61.2f;
  sys.gLight = 800; sys.gSoil = 650; sys.gWater = 900;
  sys.gPir = true; sys.gTouch = false;
  CHECK_EQ(invoke("/api"), 200);
  CHECK_STR(mockServerLastType(), "application/json");
  const char *b = mockServerLastBody();
  CHECK(has(b, "\"temp\":26.5"));
  CHECK(has(b, "\"hum\":61.2"));
  CHECK(has(b, "\"light\":800"));
  CHECK(has(b, "\"soil\":650"));
  CHECK(has(b, "\"water\":900"));
  CHECK(has(b, "\"weather\":1"));            // sáng + không khí khô -> nắng
  CHECK(has(b, "\"pir\":true"));
  CHECK(has(b, "\"touch\":false"));
  CHECK(has(b, "\"relay\":false"));
  CHECK(has(b, "\"windmill\":false"));
  CHECK(has(b, "\"feed\":false"));
  CHECK(has(b, "\"demo\":false"));
  CHECK(has(b, "\"pumpAuto\":"));
  CHECK(has(b, "\"breatheAuto\":"));
}

// I3: /on /off (hoạt động 12)
static void testHttpLedOnOff() {
  printf("I3  http: /on /off\n");
  CHECK_EQ(invoke("/on"), 200);
  CHECK(sys.blueLedState);
  CHECK(!sys.lightAuto);
  CHECK_STR(mockServerLastBody(), "LED ON");
  CHECK_EQ(invoke("/off"), 200);
  CHECK(!sys.blueLedState);
  CHECK_STR(mockServerLastBody(), "LED OFF");
}

// I4: cơ cấu servo qua HTTP (HD6/HD10)
static void testHttpMechanisms() {
  printf("I4  http: windmill / feed / servo arbitration\n");
  invoke("/windmill", "on", "true");
  CHECK(sys.windmillOn);
  invoke("/windmill", "on", "false");
  CHECK(!sys.windmillOn);

  invoke("/feed");
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);

  invoke("/servo", "angle", "45");           // điều khiển thủ công giải phóng cơ cấu
  CHECK_EQ(sys.servoAngle, 45);
  CHECK_EQ(mockServoAngle, 45);
  CHECK(!feedRunning());
  invoke("/servo", "angle", "999");          // bị kẹp
  CHECK_EQ(sys.servoAngle, 180);
  invoke("/servo", "angle", "-5");           // bị kẹp
  CHECK_EQ(sys.servoAngle, 0);

  invoke("/demo");
  CHECK(demoRunning());
  runLoop(40000);                            // chạy toàn bộ demo qua loop()
  CHECK(!demoRunning());
}

// I5: toggle / auto / rgb / threshold
static void testHttpControlRoutes() {
  printf("I5  http: toggle/auto/rgb/threshold\n");
  invoke("/toggle", "id", "pump");
  CHECK(!sys.pumpAuto);
  CHECK(sys.relayState);
  invoke("/toggle", "id", "pump");
  CHECK(!sys.relayState);
  CHECK_EQ(invoke("/toggle", "id", "bogus"), 400);

  invoke("/auto", "id", "rgb", "on", "false");
  CHECK(!sys.rgbAuto);

  invoke("/rgb", "r", "300", "g", "-5", "b", "128");
  CHECK_EQ(sys.rgbR, 255);
  CHECK_EQ(sys.rgbG, 0);
  CHECK_EQ(sys.rgbB, 128);
  CHECK(!sys.rgbAuto);
  CHECK_EQ(mockPixel[1], (255 << 16) | (0 << 8) | 128);

  invoke("/auto", "id", "breathe", "on", "true");
  CHECK(sys.breatheAuto);
  CHECK(sys.breatheLedState);
  runLoop(2000);
  uint32_t peak = 0;
  for (int i = 0; i < 60; i++) {
    mockAdvance(BREATHE_INTERVAL);
    loop();
    if (currentLedPwm() > peak) peak = currentLedPwm();
  }
  CHECK(peak > 0);                           // breathing được animate qua loop()
  invoke("/auto", "id", "breathe", "on", "false");
  CHECK(!sys.breatheAuto);
  CHECK(!sys.breatheLedState);
  CHECK_EQ(invoke("/auto", "id", "bogus", "on", "true"), 400);

  invoke("/threshold", "soilLow", "2000");
  CHECK_NEAR(sys.soilThresholdLow, 2000, 0.01);
  CHECK(sys.soilThresholdHigh > sys.soilThresholdLow);   // tự sửa thứ tự
  CHECK_NEAR(mockPrefGet("soilLow", 0), 2000, 0.01);  // đã lưu bền
  invoke("/threshold", "soilLow", "9000");
  CHECK_NEAR(sys.soilThresholdLow, 4095, 0.01);      // bị kẹp
  CHECK_NEAR(sys.soilThresholdHigh, 4095, 0.01);
  CHECK_EQ(invoke("/threshold"), 400);           // không có tham số
  sys.soilThresholdLow = 400; sys.soilThresholdHigh = 700;

  invoke("/threshold", "tempLow", "50");         // dải nhiệt độ đảo được tự sửa
  CHECK_NEAR(sys.tempThresholdLow, 50, 0.01);
  CHECK(sys.tempThresholdHigh > sys.tempThresholdLow);
  CHECK_NEAR(mockPrefGet("tempHigh", 0), sys.tempThresholdHigh, 0.01);
  sys.tempThresholdHigh = 30; sys.tempThresholdLow = 25;
}

// I6: trang gốc + favicon
static void testHttpRootFavicon() {
  printf("I6  http: root page + favicon\n");
  CHECK_EQ(invoke("/"), 200);
  CHECK_STR(mockServerLastType(), "text/html");
  CHECK(has(mockServerLastBody(), "Smart Farm"));
  CHECK(has(mockServerLastBody(), "/windmill"));
  CHECK(has(mockServerLastBody(), "/demo"));
  CHECK_EQ(invoke("/favicon.ico"), 204);
}

// I12: cấu hình WiFi động qua web (lưu flash rồi yêu cầu khởi động lại)
static void testHttpWifiConfig() {
  printf("I12 http: /wifi dynamic config\n");
  resetState();
  mockServerReset();
  setup();

  CHECK_EQ(invoke("/wifi"), 400);                        // thiếu tham số
  CHECK_STR(mockServerLastBody(), "no params");
  CHECK(!sys.gNeedRestart);

  invoke("/wifi", "ssid", "NhaToi", "password", "matkhau123");
  CHECK_STR(sys.ssid, "NhaToi");
  CHECK_STR(sys.password, "matkhau123");
  CHECK_STR(mockPrefGetStr("wifiSsid", ""), "NhaToi");
  CHECK_STR(mockPrefGetStr("wifiPass", ""), "matkhau123");
  CHECK(sys.gNeedRestart);                                   // chờ restart để áp dụng

  // loop() đi qua nhánh restart (mock ESP.restart là no-op); chạy quá 200 ms
  // để chạm đúng mốc RESTART_DELAY_MS tính từ lần loop() đầu tiên
  sys.gNeedRestart = true;
  runLoop(RESTART_DELAY_MS + 20);
  sys.gNeedRestart = false;

  // khôi phục trạng thái nền cho các test sau
  strncpy(sys.ssid, "Tab S10 Lite của Trinh", sizeof(sys.ssid) - 1);
  sys.ssid[sizeof(sys.ssid) - 1] = '\0';
  strncpy(sys.password, "99999999T", sizeof(sys.password) - 1);
  sys.password[sizeof(sys.password) - 1] = '\0';
  resetState();
  mockServerReset();
  setup();
}

// I13: callback OTA in đúng trạng thái ra serial
static void testOtaCallbacks() {
  printf("I13 ota: callbacks\n");
  resetState();
  mockServerReset();
  setup();                                               // đăng ký callback
  mockSerialClear();

  mockOtaStart();
  CHECK(has(mockSerialOutput(), "OTA: start"));
  mockOtaProgress(50, 100);
  CHECK(has(mockSerialOutput(), "OTA: 50%"));
  mockOtaError(OTA_AUTH_ERROR);
  CHECK(has(mockSerialOutput(), "Auth Failed"));
  mockOtaError(OTA_BEGIN_ERROR);
  CHECK(has(mockSerialOutput(), "Begin Failed"));
  mockOtaError(OTA_CONNECT_ERROR);
  CHECK(has(mockSerialOutput(), "Connect Failed"));
  mockOtaError(OTA_RECEIVE_ERROR);
  CHECK(has(mockSerialOutput(), "Receive Failed"));
  mockOtaError(OTA_END_ERROR);
  CHECK(has(mockSerialOutput(), "End Failed"));
  mockOtaEnd();
  CHECK(has(mockSerialOutput(), "OTA: end"));

  // khôi phục trạng thái nền cho các test sau
  resetState();
  mockServerReset();
  setup();
}

// I7: render OLED
static void testOled() {
  printf("I7  oled: status frame\n");
  mockWifiStatus = WL_CONNECTED;
  sys.gTemp = 26.5f; sys.gHum = 61; sys.gLight = 800; sys.gSoil = 650; sys.gWater = 900;
  sys.relayState = false;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  const char *t = mockOledText();
  CHECK(has(t, "Smart Farm"));
  CHECK(has(t, "TT: Nang"));
  CHECK(has(t, "T:26.5C H:61%"));
  CHECK(has(t, "Light: 800"));
  CHECK(has(t, "Soil: 650"));
  CHECK(has(t, "H2O:900 P:OFF"));
  CHECK(has(t, "IP:192.168.1.50"));
  CHECK(has(t, "San sang"));

  mockWifiStatus = WL_DISCONNECTED;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK(has(mockOledText(), "WiFi: ---"));
  mockWifiStatus = WL_CONNECTED;

  sys.windmillOn = true;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK(has(mockOledText(), "Coi xay: ON"));
  sys.windmillOn = false;
  startFeed();
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK(has(mockOledText(), "Cho an..."));
  stopFeed();
}

// I8: menu debug serial
static void testSerialMenu() {
  printf("I8  serial: debug menu commands\n");
  mockSerialClear();

  struct { char cmd; const char *marker; } cases[] = {
    {'1', "--- SENSORS ---"},
    {'2', "--- ACTUATORS ---"},
    {'3', "--- CONTROL ---"},
    {'4', "--- WEBSERVER ---"},
    {'5', "--- WIFI ---"},
    {'6', "--- OLED ---"},
    {'7', "--- SETTINGS ---"},
    {'8', "--- LOGGER ---"},
  };
  for (auto &c : cases) {
    mockSerialClear();
    char __cmd[2] = { c.cmd, '\0' };
    mockSerialPush(__cmd);
    loop();
    CHECK(has(mockSerialOutput(), c.marker));
    CHECK(has(mockSerialOutput(), "Chon: "));       // in lại menu
    mockSerialPush("0");
    loop();
    CHECK(has(mockSerialOutput(), "Running normally"));
  }

  mockSerialClear();
  mockSerialPush("x");                               // không hợp lệ
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));
  CHECK(has(mockSerialOutput(), "Chon: "));
  mockSerialClear();
}

// I9: demo qua phím serial '9'
static void testSerialDemo() {
  printf("I9  serial: '9' demo + OLED step name\n");
  mockSerialClear();
  mockSerialPush("9");
  loop();
  CHECK(demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO START"));

  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK(has(mockOledText(), "HD2: Thap sang"));      // tên bước trên OLED

  runLoop(40000);
  CHECK(!demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK(has(mockOledText(), "San sang"));
}

// I14: trình tự test cơ cấu chạy KHÔNG chặn (bấm '2', loop() tự bơm từng bước)
static void testSerialActuatorSequence() {
  printf("I14 serial: non-blocking actuator self-test\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();

  mockSerialPush("2");
  loop();                                            // bắt đầu, trả về ngay
  CHECK(has(mockSerialOutput(), "--- ACTUATORS ---"));
  CHECK(has(mockSerialOutput(), "Pump on/off..."));
  CHECK(sys.relayState);                             // bước 1: bơm bật
  CHECK(sys.fanState == false);

  mockAdvance(799);                                  // 799 ms: chưa hết bước
  loop();
  CHECK(sys.relayState);
  mockAdvance(1);                                    // đúng 800 ms: sang bước kế
  loop();
  CHECK(!sys.relayState);
  mockAdvance(400);                                  // 400 ms sau: quạt bật
  loop();
  CHECK(sys.fanState);

  // Chạy hết trình tự (~10,3 s), bơm bằng loop() 10 ms/lần
  bool sawBreathe = false, sawPlant = false;
  unsigned long cap = mockMillis() + 20000;
  while (mockMillis() < cap) {
    mockAdvance(10);
    loop();
    if (sys.breatheLedState) sawBreathe = true;
    if (sys.rgbR == 255 && sys.rgbG == 0 && sys.rgbB == 255) sawPlant = true;
    if (has(mockSerialOutput(), "Servo sweep...")) break;
  }
  CHECK(sawBreathe);                                 // HD3: bước thở đã chạy
  CHECK(sawPlant);                                   // HD9: đèn tăng trưởng đã chạy
  CHECK(has(mockSerialOutput(), "Buzzer on/off..."));// HD8: còi đã hét thử
  // Kết thúc sạch: mọi cơ cấu tắt, servo về góc gốc
  runLoop(3000);
  CHECK(!sys.relayState);
  CHECK(!sys.fanState);
  CHECK(!sys.blueLedState);
  CHECK(!sys.breatheLedState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);

  // Thoát chế độ debug để chuẩn bị phần sau
  mockSerialPush("0");
  loop();

  // Thoát debug giữa chừng: trình tự dừng ngay và tắt an toàn
  resetState();
  mockSerialClear();
  mockSerialPush("2");
  loop();
  CHECK(sys.relayState);
  mockAdvance(300);
  mockSerialPush("0");
  loop();
  CHECK(!sys.relayState);
  CHECK(has(mockSerialOutput(), "Actuator test stopped."));
  CHECK(has(mockSerialOutput(), "Running normally..."));

  setBuzzer(false);      // dọn trạng thái còi trước khi trả về nền test sạch
  resetState();
  setup();               // mockReset() xoá route đã đăng ký; các test HTTP phía sau cần lại
  mockSerialClear();
}

// I10: xử lý mất/kết nối lại WiFi
static void testWifiRecovery() {
  printf("I10 wifi: loss + reconnect logging\n");
  mockWifiStatus = WL_DISCONNECTED;
  sys.lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi lost"));
  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
  // Chờ hết cửa sổ poll trạng thái (WiFi.status() được cache) thì firmware
  // mới nhận ra đã kết nối lại
  mockAdvance(WIFI_STATUS_POLL_MS);
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
}

// ==============================================
//  Deep integration test
// ==============================================

// Parser flat-JSON tối giản dùng để kiểm tra contract của /api.
struct JsonPair { std::string key, raw, type; };   // kiểu: s(tring) n(umber) b(ool) z(null)

static bool parseFlatJson(const char *text, std::vector<JsonPair> &out, std::string &err) {
  const char *p = text;
  auto skipWs = [&]() { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; };
  skipWs();
  if (*p != '{') { err = "not a JSON object"; return false; }
  p++; skipWs();
  if (*p == '}') {
    p++; skipWs();
    if (*p != '\0') { err = "trailing garbage"; return false; }
    return true;
  }
  while (true) {
    skipWs();
    if (*p != '"') { err = "key is not a string"; return false; }
    p++;
    std::string key;
    while (*p && *p != '"') key += *p++;
    if (*p != '"') { err = "unterminated key"; return false; }
    p++; skipWs();
    if (*p != ':') { err = "missing ':'"; return false; }
    p++; skipWs();

    JsonPair pair;
    pair.key = key;
    if (*p == '"') {
      p++;
      pair.type = "s";
      while (*p && *p != '"') {
        if (*p == '\\') {
          p++;
          if (!*p) { err = "bad escape"; return false; }
        }
        pair.raw += *p++;
      }
      if (*p != '"') { err = "unterminated string"; return false; }
      p++;
    } else if (strncmp(p, "true", 4) == 0) {
      pair.type = "b"; pair.raw = "true"; p += 4;
    } else if (strncmp(p, "false", 5) == 0) {
      pair.type = "b"; pair.raw = "false"; p += 5;
    } else if (strncmp(p, "null", 4) == 0) {
      pair.type = "z"; pair.raw = "null"; p += 4;
    } else {
      char *end = nullptr;
      double v = strtod(p, &end);
      if (end == p) { err = "invalid value"; return false; }
      (void)v;
      pair.type = "n";
      pair.raw.assign(p, end - p);
      p = end;
    }
    out.push_back(pair);
    skipWs();
    if (*p == ',') { p++; continue; }
    if (*p == '}') { p++; break; }
    err = "expected ',' or '}'";
    return false;
  }
  skipWs();
  if (*p != '\0') { err = "trailing garbage"; return false; }
  return true;
}

struct ApiKey { const char *key; char type; };
static const ApiKey API_KEYS[] = {
  {"temp", 'n'}, {"hum", 'n'}, {"light", 'n'}, {"soil", 'n'}, {"water", 'n'}, {"weather", 'n'},
  {"pir", 'b'}, {"touch", 'b'}, {"relay", 'b'}, {"fan", 'b'}, {"led", 'b'}, {"breathe", 'b'},
  {"windmill", 'b'}, {"feed", 'b'}, {"demo", 'b'}, {"demoStep", 's'},
  {"pumpAuto", 'b'}, {"lightAuto", 'b'}, {"rgbAuto", 'b'}, {"fanAuto", 'b'}, {"breatheAuto", 'b'},
  {"nonce", 'n'}
};
static const int API_KEY_COUNT = (int)(sizeof(API_KEYS) / sizeof(API_KEYS[0]));

static const JsonPair *findPair(const std::vector<JsonPair> &pairs, const char *key) {
  for (const auto &p : pairs) if (p.key == key) return &p;
  return nullptr;
}

static double numOf(const std::vector<JsonPair> &pairs, const char *key) {
  const JsonPair *p = findPair(pairs, key);
  return p ? strtod(p->raw.c_str(), nullptr) : -1e9;
}

static bool boolOf(const std::vector<JsonPair> &pairs, const char *key) {
  const JsonPair *p = findPair(pairs, key);
  return p && p->raw == "true";
}

// Kiểm tra contract đầy đủ cho response /api cuối cùng
static bool validateApiResponse(std::string &err) {
  if (mockServerLastCode() != 200) { err = "HTTP code != 200"; return false; }
  if (strcmp(mockServerLastType(), "application/json") != 0) { err = "wrong content type"; return false; }

  std::vector<JsonPair> pairs;
  if (!parseFlatJson(mockServerLastBody(), pairs, err)) return false;

  if ((int)pairs.size() != API_KEY_COUNT) {
    char b[64]; snprintf(b, sizeof(b), "key count %d != %d", (int)pairs.size(), API_KEY_COUNT);
    err = b;
    return false;
  }
  std::set<std::string> seen;
  for (const auto &p : pairs) {
    if (!seen.insert(p.key).second) { err = "duplicate key: " + p.key; return false; }
  }
  for (int i = 0; i < API_KEY_COUNT; i++) {
    const JsonPair *p = findPair(pairs, API_KEYS[i].key);
    if (!p) { err = std::string("missing key: ") + API_KEYS[i].key; return false; }
    if (p->type[0] != API_KEYS[i].type) { err = std::string("bad type: ") + API_KEYS[i].key; return false; }
  }

  if (fabs(numOf(pairs, "temp") - sys.gTemp) > 0.06) { err = "temp mismatch"; return false; }
  if (fabs(numOf(pairs, "hum") - sys.gHum) > 0.06) { err = "hum mismatch"; return false; }
  if ((long)numOf(pairs, "light") != sys.gLight) { err = "light mismatch"; return false; }
  if ((long)numOf(pairs, "soil") != sys.gSoil) { err = "soil mismatch"; return false; }
  if ((long)numOf(pairs, "water") != sys.gWater) { err = "water mismatch"; return false; }
  if ((int)numOf(pairs, "weather") != (int)weatherStatus()) { err = "weather mismatch"; return false; }

  if (boolOf(pairs, "pir") != sys.gPir) { err = "pir mismatch"; return false; }
  if (boolOf(pairs, "touch") != sys.gTouch) { err = "touch mismatch"; return false; }
  if (boolOf(pairs, "relay") != sys.relayState) { err = "relay mismatch"; return false; }
  if (boolOf(pairs, "fan") != sys.fanState) { err = "fan mismatch"; return false; }
  if (boolOf(pairs, "led") != sys.blueLedState) { err = "led mismatch"; return false; }
  if (boolOf(pairs, "breathe") != sys.breatheLedState) { err = "breathe mismatch"; return false; }
  if (boolOf(pairs, "windmill") != sys.windmillOn) { err = "windmill mismatch"; return false; }
  if (boolOf(pairs, "feed") != feedRunning()) { err = "feed mismatch"; return false; }
  if (boolOf(pairs, "demo") != demoRunning()) { err = "demo mismatch"; return false; }
  if (boolOf(pairs, "pumpAuto") != sys.pumpAuto) { err = "pumpAuto mismatch"; return false; }
  if (boolOf(pairs, "lightAuto") != sys.lightAuto) { err = "lightAuto mismatch"; return false; }
  if (boolOf(pairs, "rgbAuto") != sys.rgbAuto) { err = "rgbAuto mismatch"; return false; }
  if (boolOf(pairs, "fanAuto") != sys.fanAuto) { err = "fanAuto mismatch"; return false; }
  if (boolOf(pairs, "breatheAuto") != sys.breatheAuto) { err = "breatheAuto mismatch"; return false; }

  const JsonPair *ds = findPair(pairs, "demoStep");
  const char *expectedStep = demoRunning() ? demoStepName() : "";
  if (ds->raw != expectedStep) { err = "demoStep mismatch"; return false; }

  return true;
}

// D5: JSON contract nghiêm ngặt của /api
static void testApiJsonContract() {
  printf("D5  api: strict JSON contract (21 keys)\n");
#if API_JSON_IS_TINY
  CHECK_EQ(invoke("/api"), 500);                 // buffer JSON nhỏ -> báo lỗi
  CHECK(has(mockServerLastBody(), "too large"));
  return;
#endif
  std::string err;

  // Trạng thái đã biết
  sys.gTemp = 26.5f; sys.gHum = 61.2f;
  sys.gLight = 800; sys.gSoil = 650; sys.gWater = 900;
  sys.gPir = true; sys.gTouch = false;
  invoke("/api");
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());

  // Trạng thái demo: cờ demo + chuỗi step phải cập nhật trực tiếp
  invoke("/demo");
  CHECK(demoRunning());
  invoke("/api");
  err.clear();
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());
  CHECK(has(mockServerLastBody(), "\"demo\":true"));
  CHECK(has(mockServerLastBody(), demoStepName()));
  runLoop(40000);
  CHECK(!demoRunning());

  // Trạng thái cho ăn
  startFeed();
  invoke("/api");
  err.clear();
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());
  CHECK(has(mockServerLastBody(), "\"feed\":true"));
  stopFeed();

  // 200 trạng thái ngẫu nhiên: JSON phải luôn hợp lệ và nhất quán
  std::mt19937 rng(1234);
  int bad = 0;
  for (int i = 0; i < 200; i++) {
    sys.gTemp = (float)(rng() % 500) / 10.0f;
    sys.gHum = (float)(rng() % 101);
    sys.gLight = (int)(rng() % 4096);
    sys.gSoil = (int)(rng() % 4096);
    sys.gWater = (int)(rng() % 4096);
    sys.gPir = (rng() % 2) != 0;
    sys.gTouch = (rng() % 2) != 0;
    invoke("/api");
    err.clear();
    if (!validateApiResponse(err)) {
      bad++;
      if (bad == 1) printf("  FAIL: %s\n", err.c_str());
    }
  }
  CHECK_EQ(bad, 0);
}

// D6: độ chính xác body HTTP + biên ngưỡng
static void testResponseExactness() {
  printf("D6  http: exact bodies + threshold edges\n");

  CHECK_EQ(invoke("/on"), 200);
  CHECK_STR(mockServerLastBody(), "LED ON");
  CHECK_STR(mockServerLastType(), "text/plain");
  CHECK_EQ(invoke("/off"), 200);
  CHECK_STR(mockServerLastBody(), "LED OFF");

  CHECK_EQ(invoke("/toggle", "id", "pump"), 200);
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK_EQ(invoke("/toggle", "id", "nope"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");
  CHECK_EQ(invoke("/auto", "id", "nope", "on", "true"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");
  CHECK_EQ(invoke("/auto", "id", "light", "on", "TRUE"), 200);
  CHECK(!sys.lightAuto);                       // chỉ "true" mới bật auto

  invoke("/rgb", "r", "0", "g", "255", "b", "255");
  CHECK_EQ(sys.rgbR, 0); CHECK_EQ(sys.rgbG, 255); CHECK_EQ(sys.rgbB, 255);
  CHECK_STR(mockServerLastBody(), "ok");

  invoke("/breathe");
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK(sys.breatheLedState);
  CHECK(!sys.breatheAuto);
  invoke("/breathe");
  CHECK(!sys.breatheLedState);

  invoke("/windmill");                     // không tham số -> tắt
  CHECK_STR(mockServerLastBody(), "windmill off");
  invoke("/windmill", "on", "true");
  CHECK_STR(mockServerLastBody(), "windmill on");
  CHECK(sys.windmillOn);
  invoke("/windmill", "on", "false");
  CHECK_STR(mockServerLastBody(), "windmill off");

  invoke("/feed");
  CHECK_STR(mockServerLastBody(), "feeding");
  CHECK(feedRunning());
  runLoop(2100);
  CHECK(!feedRunning());

  invoke("/demo");
  CHECK_STR(mockServerLastBody(), "demo started");
  CHECK(demoRunning());
  runLoop(32000);
  CHECK(!demoRunning());

  CHECK_EQ(invoke("/threshold"), 400);
  CHECK_STR(mockServerLastBody(), "no params");
  invoke("/threshold", "soilLow", "4095");
  CHECK_NEAR(sys.soilThresholdLow, 4095, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 4095, 0.01);        // mức sửa tốt nhất tại biên
  // Ngưỡng bằng nhau không được làm bơm bật/tắt liên tục (guard điều khiển):
  sys.gWater = 900; sys.gSoil = 100; sys.relayState = false; sys.pumpAuto = true;
  controlPump();
  CHECK(!sys.relayState);
  sys.soilThresholdLow = 400; sys.soilThresholdHigh = 700;

  invoke("/threshold", "soilLow", "-5");
  CHECK_NEAR(sys.soilThresholdLow, 0, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 700, 0.01);         // không đổi (vẫn > low)
  invoke("/threshold", "soilLow", "abc");
  CHECK_NEAR(sys.soilThresholdLow, 0, 0.01);            // toFloat("abc") trả về 0
  sys.soilThresholdLow = 400; sys.soilThresholdHigh = 700;

  invoke("/threshold", "waterLow", "99999");
  CHECK_NEAR(sys.waterThresholdLow, 4095, 0.01);
  sys.waterThresholdLow = 200;

  invoke("/threshold", "tempLow", "60");
  CHECK_NEAR(sys.tempThresholdLow, 60, 0.01);
  CHECK_NEAR(sys.tempThresholdHigh, 61, 0.01);          // giữ nghiêm ngặt trên low
  invoke("/threshold", "tempLow", "25");
  CHECK_NEAR(sys.tempThresholdLow, 25, 0.01);
  invoke("/threshold", "tempHigh", "0");
  CHECK_NEAR(sys.tempThresholdHigh, 26, 0.01);          // sửa thành low + 1
  sys.tempThresholdHigh = 30; sys.tempThresholdLow = 25;
  invoke("/threshold", "light", "5000");
  CHECK_EQ(sys.lightThreshold, 4095);
  sys.lightThreshold = 500;

  invoke("/rgb");                                    // không tham số -> toàn số 0
  CHECK_EQ(sys.rgbR, 0); CHECK_EQ(sys.rgbG, 0); CHECK_EQ(sys.rgbB, 0);
  CHECK(!sys.rgbAuto);
  invoke("/servo", "angle", "0");
  CHECK_EQ(sys.servoAngle, 0);
  invoke("/servo", "angle", "180");
  CHECK_EQ(sys.servoAngle, 180);
  invoke("/servo", "angle", "abc");
  CHECK_EQ(sys.servoAngle, 0);
  sys.servoAngle = 0;
}

// D7: bố cục từng dòng OLED + giới hạn độ dài dòng
static void testOledLines() {
  printf("D7  oled: per-row layout + 21-char line limit\n");
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "255.255.255.255";
  sys.gTemp = 26.5f; sys.gHum = 61; sys.gLight = 800; sys.gSoil = 650; sys.gWater = 900;
  sys.relayState = false; sys.windmillOn = false;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_STR(mockOledLine(0), "Smart Farm");
  CHECK_STR(mockOledLine(1), "TT: Nang");
  CHECK_STR(mockOledLine(2), "T:26.5C H:61%");
  CHECK_STR(mockOledLine(3), "Light: 800");
  CHECK_STR(mockOledLine(4), "Soil: 650");
  CHECK_STR(mockOledLine(5), "H2O:900 P:OFF");
  CHECK_STR(mockOledLine(6), "IP:255.255.255.255");
  CHECK_STR(mockOledLine(7), "San sang");

  // Giá trị xấu nhất: mỗi dòng vẫn phải vừa 21 ký tự (128 px / 6 px)
  sys.gTemp = 99.9f; sys.gHum = 100; sys.gLight = 4095; sys.gSoil = 4095; sys.gWater = 4095;
  sys.relayState = true;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  for (int r = 0; r < mockOledLineCount(); r++) {
    CHECK(strlen(mockOledLine(r)) <= 21);
    CHECK(strlen(mockOledLine(r)) > 0);
  }
  CHECK_STR(mockOledLine(5), "H2O:4095 P:ON");
  sys.relayState = false;
  mockWifiIp = "192.168.1.50";
}

// D20: OLED chỉ đẩy I2C vùng thực sự thay đổi (delta theo dải cột trong
// từng page 8-hàng), không gửi đủ 1024 byte
static void testOledDelta() {
  printf("D20 oled: delta page updates over I2C\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "192.168.1.50";
  sys.gTemp = 26.5f; sys.gHum = 61; sys.gLight = 800; sys.gSoil = 650; sys.gWater = 900;

  // Khung đầu tiên sau khi bật: phải gửi đủ 8 page = 1024 byte dữ liệu
  int t0 = mockI2cTransactions, c0 = mockI2cCmdBytes, d0 = mockI2cDataBytes;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_EQ(mockI2cDataBytes - d0, 1024);
  CHECK_EQ(mockI2cCmdBytes - c0, 16);           // 7 byte lệnh + 9 byte 0x40 (1/chunk)
  CHECK_EQ(mockI2cTransactions - t0, 10);       // 1 lệnh + 9 chunk dữ liệu (127×8+8)

  // Không có gì đổi: không chạm I2C
  t0 = mockI2cTransactions; d0 = mockI2cDataBytes;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_EQ(mockI2cDataBytes - d0, 0);
  CHECK_EQ(mockI2cTransactions - t0, 0);

  // Mực nước đổi -> chỉ 1 ký tự trong dòng H2O đổi ('9'->'5' tại cột 24..29).
  // Font 6x8: mỗi ký tự chiếm 6 cột -> chỉ 6 byte dữ liệu thay vì 128 byte
  // của cả page (giảm 95% lưu lượng I2C, rút ngắn thời gian chặn loop).
  sys.gWater = 500;
  t0 = mockI2cTransactions; d0 = mockI2cDataBytes;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_EQ(mockI2cDataBytes - d0, 6);
  CHECK_EQ(mockI2cTransactions - t0, 2);        // 1 lệnh địa chỉ + 1 gói dữ liệu
  CHECK_STR(mockOledLine(5), "H2O:500 P:OFF");

  // Nhiệt độ (page 2) + độ ẩm đất (page 4) đổi -> mỗi page gửi đúng dải cột
  // của các ký tự đổi: page 2 đổi 3 ký tự (cột 12..35 = 24 byte), page 4 đổi
  // 3 ký tự (cột 36..53 = 18 byte) -> 42 byte thay vì 384 byte (giảm 89%).
  sys.gTemp = 30.0f; sys.gSoil = 123;
  t0 = mockI2cTransactions; d0 = mockI2cDataBytes;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_EQ(mockI2cDataBytes - d0, 42);
  CHECK_EQ(mockI2cTransactions - t0, 4);        // 2 lệnh địa chỉ + 2 gói dữ liệu

  // Ép vẽ lại toàn bộ (code khác đã ghi đè màn hình) -> lại đủ 1024 byte
  oledForceRedraw();
  t0 = mockI2cTransactions; d0 = mockI2cDataBytes;
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_EQ(mockI2cDataBytes - d0, 1024);
  CHECK_EQ(mockI2cTransactions - t0, 10);

  // Trả trạng thái sạch cho test kế tiếp (testSerialEdges gọi loop() và đòi
  // hỏi không có log/automation nào phát sinh).
  resetState();
}

// D20b: OLED vắng mặt / I2C hỏng — firmware phải ngừng đụng I2C
// (mỗi giao dịch I2C lỗi chặn loop tới hết thời gian chờ).
static void testOledAbsentOrBroken() {
  printf("D20b oled: absent display + broken I2C self-disable\n");
  resetState();
  mockServerReset();
  mockDisplayBeginOk = false;             // không có màn hình
  mockSerialClear();
  setup();

  CHECK(!oledAvailable());                // oledInit() báo không có màn hình
  CHECK(has(mockSerialOutput(), "MA LOI: 101"));
  const int t0 = mockI2cTransactions;
  updateOLED(25, 60, 500, 400, 800);      // vẽ -> không được đụng I2C
  CHECK_EQ(mockI2cTransactions - t0, 0);

  // Có màn hình nhưng bus I2C hỏng giữa chừng -> sau vài lần lỗi thì tự tắt
  mockDisplayBeginOk = true;
  resetState();
  mockServerReset();
  setup();
  CHECK(oledAvailable());

  updateOLED(25, 60, 500, 400, 800);      // vẽ thành công -> khung so sánh hợp lệ
  CHECK(oledAvailable());

  mockI2cFailCount = 1000;                // mọi giao dịch đều lỗi
  for (int i = 0; i < 3; i++) {
    sys.gTemp = 20.0f + i;                // ép vẽ lại để có dữ liệu mới
    updateOLED(sys.gTemp, 60, 500, 400, 800);
  }
  CHECK(!oledAvailable());                // đã tự tắt màn hình
  const int t1 = mockI2cTransactions;
  updateOLED(30, 60, 500, 400, 800);      // đã tắt -> không đụng I2C nữa
  CHECK_EQ(mockI2cTransactions - t1, 0);

  // Lệnh debug cũng phải bỏ qua khi không có màn hình
  mockSerialClear();
  debugOled();
  CHECK(has(mockSerialOutput(), "OLED not available"));

  mockI2cFailCount = 0;
  mockDisplayBeginOk = true;
  resetState();
  mockServerReset();
  setup();
}

// D21: JSON /api bị cắt cụt khi buffer quá nhỏ -> phải báo lỗi, không trả JSON hỏng
static void testApiJsonTruncation() {
  printf("D21 api: truncated JSON is reported as error\n");
  char tiny[16];
  const int need = testBuildApiJson(tiny, sizeof(tiny));
  CHECK(need > (int)sizeof(tiny));              // snprintf báo cần nhiều hơn buffer
  CHECK(strlen(tiny) == sizeof(tiny) - 1);      // buffer đầy, không tràn
#if API_JSON_IS_TINY
  CHECK_EQ(invoke("/api"), 500);
  CHECK(has(mockServerLastBody(), "too large"));
#else
  CHECK_EQ(invoke("/api"), 200);                 // buffer mặc định: JSON vừa khít
#endif
}

// D22: OLED cập nhật NGAY khi trạng thái hiển thị đổi, không phải chờ 2 giây
static void testOledDirtyRefresh() {
  printf("D22 oled: refresh ngay khi trạng thái đổi\n");
  resetState();
  mockServerReset();
  setup();

  updateOLED(25, 60, 500, 400, 800);      // vẽ nền
  sys.lastOledUpdate = mockMillis();
  const int t0 = mockI2cTransactions;

  // Không có gì đổi -> không vẽ lại, không tốn byte I2C nào
  mockAdvance(100);
  loop();
  CHECK_EQ(mockI2cTransactions - t0, 0);
  CHECK(!sys.oledDirty);

  // Bật cối xay -> dòng trạng thái trên màn hình đổi -> vẽ lại ngay
  setWindmill(true);
  CHECK(sys.oledDirty);
  mockAdvance(1);
  loop();
  CHECK(mockI2cTransactions - t0 > 0);      // đã vẽ lại
  CHECK(!sys.oledDirty);                   // cờ đã được xoá sau khi vẽ
  CHECK(has(mockOledText(), "Coi xay: ON"));

  resetState();
  mockServerReset();
  setup();
}

// D23: ngưỡng đọc từ flash ngoài khoảng hợp lệ phải được chặn lại
static void testSettingsRangeClamp() {
  printf("D23 settings: clamp nguong doc tu flash\n");
  resetState();
  mockPrefClear();
  mockPrefPut("soilLow", 99999.0f);        // flash hỏng / ghi tay sai
  mockPrefPut("soilHigh", -5.0f);
  mockPrefPut("waterLow", 1e9f);
  mockPrefPut("lightThr", 88888.0f);
  mockPrefPut("tempHigh", 500.0f);
  mockPrefPut("tempLow", -100.0f);

  loadSettings();
  CHECK_NEAR(sys.soilThresholdLow, 4095.0f, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 0.0f, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 4095.0f, 0.01);
  CHECK_EQ(sys.lightThreshold, 4095);
  CHECK_NEAR(sys.tempThresholdHigh, 60.0f, 0.01);
  CHECK_NEAR(sys.tempThresholdLow, 0.0f, 0.01);

  resetState();
  mockServerReset();
  setup();
}

// D8: các ca biên serial + hành vi debug-mode
static void testSerialEdges() {
  printf("D8  serial: edge cases + debug mode\n");

  // loop() đầu "nhả" các việc một-lần (thông báo IP, chốt trạng thái chạm...)
  // để test không phụ thuộc test chạy trước đó.
  loop();
  mockSerialClear();
  mockSerialPush("\r\n");                    // dòng trống: bỏ qua
  loop();
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  mockSerialClear();
  mockSerialPush("\r");                      // chỉ CR: bỏ qua
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  char badByte[2] = {(char)0xFF, 0};         // byte rác: bị từ chối
  mockSerialClear();
  mockSerialPush(badByte);
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));

  mockSerialClear();
  for (int i = 0; i < 64; i++) mockSerialPush("x");   // dòng rác dài
  for (int i = 0; i < 64; i++) loop();
  CHECK_EQ(mockSerialPending(), 0);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "Invalid"), 64);

  // '0' ở chế độ thường được xử lý an toàn (in thông báo thoát, vẫn ở chế độ thường)
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), ">> Exit debug."));

  // Chế độ debug tạm dừng automation (chế độ bảo trì)...
  resetState();
  sys.gWater = 900; sys.gSoil = 100; sys.pumpAuto = true;
  controlPump();
  CHECK(sys.relayState);
  mockSerialClear();
  mockSerialPush("3");
  loop();
  CHECK(has(mockSerialOutput(), "--- CONTROL ---"));
  sys.gWater = 50;
  mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
  sys.lastSensorRead = mockMillis();
  loop();
  CHECK(sys.relayState);                          // chế độ debug: automation tạm dừng
  // trong chế độ debug mọi phím khác đều bị từ chối
  mockSerialClear();
  mockSerialPush("x");
  loop();
  CHECK(has(mockSerialOutput(), "Enter '0' to exit debug."));
  // ...và loop đầu tiên sau khi thoát khôi phục quy tắc an toàn
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), "Running normally..."));
  CHECK(!sys.relayState);

  // Menu debug '5' khi WiFi mất sẽ đi qua nhánh kết nối lại
  resetState();
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  mockSerialPush("5");
  loop();
  CHECK(has(mockSerialOutput(), "--- WIFI ---"));
  CHECK_EQ(mockWifiReconnectCalls, 1);
  mockSerialPush("0");
  loop();
  mockWifiStatus = WL_CONNECTED;

  // Mức DEBUG bật dòng cảm biến định kỳ và tag DEBUG
  resetState();
  gMinLogLevel = LOG_DEBUG;
  mockSerialClear();
  logSensors();
  CHECK(has(mockSerialOutput(), "[SENSORS]"));
  debugLogger();
  CHECK(has(mockSerialOutput(), "[DEBUG]"));
  gMinLogLevel = LOG_INFO;

  resetState();
}

// D9: chẩn đoán demo (thứ tự, thời lượng, tên OLED, an toàn giữa bước)
static const char *DEMO_STEPS[] = {
  "HD2: Thap sang", "HD3: Den tho PWM", "HD4: Cham LED (bat)", "HD4: Cham LED (tat)",
  "HD5: PIR co nguoi", "HD5: Nguoi roi di", "HD6: Coi xay gio quay", "HD7: Tuoi tu dong",
  "HD8: Bao nuoc can", "HD9: Den tang truong", "HD10: Cho an tu dong", "HD10: Dong cua",
  "HD11: Xem OLED", "HD12: Dieu khien", "HD12: Tat tu xa"
};
static const int DEMO_STEP_COUNT = (int)(sizeof(DEMO_STEPS) / sizeof(DEMO_STEPS[0]));

static void testDemoDiagnostics() {
  printf("D9  demo: step order, duration, OLED names, mid-step safety\n");

  // Lượt 1: chuỗi bước chính xác + tổng thời lượng + tên OLED
  resetState();
  primeSensors(900);
  mockSerialClear();
  unsigned long t0 = mockMillis();
  startDemo();
  CHECK(demoRunning());
  std::vector<std::string> names;
  names.push_back(demoStepName());
  updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  CHECK_STR(mockOledLine(7), names.back().c_str());
  CHECK(strlen(names.back().c_str()) <= 21);

  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    runAutomation();
    if (!demoRunning()) break;
    const char *n = demoStepName();
    if (names.back() != n) {
      names.push_back(n);
      updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
      CHECK_STR(mockOledLine(7), n);
      CHECK(strlen(n) <= 21);
    }
  }
  unsigned long elapsed = mockMillis() - t0;
  CHECK(!demoRunning());
  CHECK_EQ((int)names.size(), DEMO_STEP_COUNT);
  for (int i = 0; i < DEMO_STEP_COUNT && i < (int)names.size(); i++) {
    CHECK_STR(names[i].c_str(), DEMO_STEPS[i]);
  }
  CHECK(elapsed >= 30700 && elapsed <= 30800);       // tổng thời gian các bước chính xác
  CHECK_EQ(countOccurrences(mockSerialOutput(), "[DEMO] "), DEMO_STEP_COUNT);
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  CHECK(!sys.relayState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);

  // Lượt 2: hết nước giữa bước bơm -> an toàn phải ngắt bơm ngay
  resetState();
  primeSensors(900);
  startDemo();
  bool dropped = false, violated = false, sawPumpOn = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    if (!dropped && strstr(demoStepName(), "HD7")) {
      dropped = true;
      sys.gWater = 50;                                     // bồn cạn giữa bước
      mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
      sys.lastSensorRead = mockMillis();
    }
    runAutomation();
    if (sys.relayState) sawPumpOn = true;
    if (sys.relayState && sys.gWater < (int)sys.waterThresholdLow) violated = true;
  }
  CHECK(dropped);
  CHECK(sawPumpOn);                                    // bơm chạy khi nước còn ổn
  CHECK(!violated);                                    // sau đó không bao giờ chạy khô
  CHECK(!demoRunning());
  CHECK(!sys.relayState);
}

// D10: bộ đếm throttle kết nối lại WiFi
static void testWifiThrottle() {
  printf("D10 wifi: reconnect throttle + counters\n");
  mockWifiStatus = WL_CONNECTED;
  const int c0 = mockWifiBeginCalls;
  loop();                                              // reset cờ đã cảnh báo
  CHECK_EQ(mockWifiBeginCalls, c0);                    // đã kết nối: không thử kết nối lại

  mockWifiStatus = WL_DISCONNECTED;
  sys.lastWifiCheck = mockMillis();
  const int b0 = mockWifiBeginCalls, d0 = mockWifiDisconnectCalls;
  mockSerialClear();

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                    // chưa đến hạn
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);                // đến hạn đúng tại khoảng thời gian
  CHECK_EQ(mockWifiDisconnectCalls, d0 + 1);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // thử lại mỗi 10 s
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);  // nhưng chỉ ghi log một lần

  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
  // Trạng thái WiFi được cache WIFI_STATUS_POLL_MS (WiFi.status() gọi vào
  // driver WiFi qua khóa dùng chung với stack WiFi) — phải chờ hết cửa sổ
  // poll thì firmware mới hỏi lại driver.
  mockAdvance(WIFI_STATUS_POLL_MS);
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // không thử thêm

  // Một lần mất mạng mới lại ghi log (cờ cảnh báo đã được reset bởi lần kết nối lại)
  mockWifiStatus = WL_DISCONNECTED;
  sys.lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  mockAdvance(WIFI_STATUS_POLL_MS);
  loop();
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);
  mockWifiStatus = WL_CONNECTED;
  mockAdvance(WIFI_STATUS_POLL_MS);
  loop();
}

// D10b: tần số hỏi trạng thái WiFi — không hỏi driver ở tần số của loop()
static void testWifiStatusPoll() {
  printf("D10b wifi: status poll is throttled\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;

  // Một lần hỏi để nạp cache ban đầu
  manageWiFi();
  const int s0 = mockWifiStatusCalls;
  CHECK_EQ(s0, 1);

  // 100 vòng loop liên tiếp (100 ms) — vẫn nằm trong cửa sổ poll
  // -> KHÔNG hỏi driver thêm (trước đây mỗi vòng loop hỏi 1 lần)
  for (int i = 0; i < 100; i++) {
    mockAdvance(1);
    loop();
  }
  CHECK_EQ(mockWifiStatusCalls, s0);

  // Sau khi hết cửa sổ poll thì hỏi lại
  mockAdvance(WIFI_STATUS_POLL_MS);
  manageWiFi();
  CHECK_EQ(mockWifiStatusCalls, s0 + 1);

  resetState();
}

// D10c: tần số xử lý gói OTA — không đọc UDP ở tần số của loop()
static void testOtaHandleThrottle() {
  printf("D10c ota: handle() is throttled\n");
  resetState();
  mockServerReset();
  setup();

  const int h0 = mockOtaHandleCalls;
  // 100 vòng loop trong 100 ms -> OTA chỉ được xử lý ở mốc OTA_HANDLE_INTERVAL
  for (int i = 0; i < 100; i++) {
    mockAdvance(1);
    loop();
  }
  const int calls = mockOtaHandleCalls - h0;
  CHECK(calls >= 1);
  CHECK(calls <= 100 / (int)OTA_HANDLE_INTERVAL + 2);   // không quá ~5 lần/100 ms

  // Sau khi hết cửa sổ thì lại xử lý
  mockAdvance(OTA_HANDLE_INTERVAL);
  loop();
  CHECK(mockOtaHandleCalls > h0 + calls);

  resetState();
  mockServerReset();
  setup();
}

// D11: thứ tự chống glitch khi boot + khởi tạo watchdog
static void testBootOrder() {
  printf("D11 boot: output latch before pinMode + WDT\n");
  mockReset();
  mockServerReset();
  mockIoLogClear();
  setup();
  checkRelayPin(false);                                // bơm TẮT khi boot

  const char *log = mockIoLog();
  const char *wRelay = strstr(log, "W:26=");
  const char *mRelay = strstr(log, "M:26;");
  CHECK(wRelay && mRelay);
  CHECK(wRelay < mRelay);                              // ghi chốt trước

  const char *wFan = strstr(log, "W:14=");
  const char *mFan = strstr(log, "M:14;");
  CHECK(wFan && mFan && wFan < mFan);

  const char *wBuzz = strstr(log, "W:25=");
  const char *mBuzz = strstr(log, "M:25;");
  CHECK(wBuzz && mBuzz && wBuzz < mBuzz);

  CHECK(mockWdtInitCalls >= 1);
  CHECK_EQ(mockWdtAddCalls, 1);
  CHECK_EQ(mockWifiPersistentCalls, 1);   // không để WiFi tự ghi creds xuống NVS
  CHECK_EQ(mockWifiSleepDisableCalls, 1); // tắt modem-sleep cho web phản hồi nhanh
  checkAllRoutes();
}

// D12: settings sống sót qua chu kỳ nguồn mô phỏng
static void testRebootPersistence() {
  printf("D12 settings: power-cycle persistence\n");
  invoke("/threshold", "soilLow", "1234");
  CHECK_NEAR(sys.soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 1284, 0.01);
  invoke("/auto", "id", "pump", "on", "false");
  invoke("/auto", "id", "breathe", "on", "true");
  invoke("/toggle", "id", "fan");
  CHECK(!sys.pumpAuto);
  CHECK(sys.breatheAuto);
  CHECK(!sys.fanAuto);
  CHECK(sys.fanState);

  // chu kỳ nguồn: flash giữ prefs, trạng thái chỉ trong RAM bị mất
  mockServerReset();
  sys.fanState = sys.relayState = sys.windmillOn = sys.blueLedState = sys.breatheLedState = false;
  sys.rgbR = sys.rgbG = sys.rgbB = 0;
  setup();

  checkAllRoutes();
  CHECK_NEAR(sys.soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 1284, 0.01);
  CHECK(!sys.pumpAuto);
  CHECK(!sys.fanAuto);
  CHECK(sys.breatheAuto);
  CHECK(sys.breatheLedState);                              // setup bật lại breathing
  CHECK(!sys.fanState);                                    // trạng thái runtime được reset
  CHECK_EQ(sys.rgbR, 0);                                   // RGB thủ công không được lưu bền
}

// D13: fuzz/soak có seed với các safety invariant
static void checkFuzzInvariants(bool demoAtLoopStart) {
  // 1. Bơm không bao giờ chạy khi hết nước (safety line + controlPump).
  CHECK(!(sys.gWater < (int)sys.waterThresholdLow && sys.relayState));
  // 2. Mức chân luôn khớp với trạng thái logic.
  CHECK((mockPinLevel[RELAY_PIN] != 0) == (sys.relayState != (bool)RELAY_ACTIVE_LOW));
  CHECK((mockPinLevel[FAN_PIN] != 0) == sys.fanState);
  // 3. Các cơ cấu servo dùng chung không bao giờ chạy cùng lúc.
  CHECK(!(sys.windmillOn && feedRunning()));
  // 4. Các chế độ LED xanh loại trừ lẫn nhau.
  CHECK(!(sys.blueLedState && sys.breatheLedState));
  // 5. Ghi servo và PWM luôn nằm trong dải hợp lệ.
  CHECK(mockServoMinSeen >= 0 && mockServoMaxSeen <= 180);
  CHECK_EQ(mockInvalidPixelWrites, 0);
  CHECK_EQ(mockInvalidPwmWrites, 0);
  // 6. Không còn PWM cũ khi cả hai chế độ LED đều tắt.
  if (!sys.blueLedState && !sys.breatheLedState) CHECK_EQ(currentLedPwm(), 0);
  // 7. Khi còi đang báo động (không phải demo), còi phải thực sự bật.
  // Bỏ qua loop mà demo đang chạy vừa kết thúc: automation (và
  // còi báo) chỉ hoạt động lại ở loop kế tiếp.
  if (!demoRunning() && !demoAtLoopStart && sys.gWater < (int)sys.waterThresholdLow) {
#if BUZZER_PASSIVE
    CHECK(mockBuzzerToneHz > 0);
#else
    CHECK_EQ(mockPinLevel[BUZZER_PIN], HIGH);
#endif
  }
}

static void fuzzRun(unsigned seed, int iters) {
  std::mt19937 rng(seed);
  const char *routes[] = {"/toggle", "/auto", "/rgb", "/servo", "/windmill", "/feed",
                          "/demo", "/on", "/off", "/api", "/", "/favicon.ico", "/threshold"};
  const char *argNames[] = {"id", "on", "r", "g", "b", "angle",
                            "soilLow", "soilHigh", "waterLow", "light", "tempHigh", "tempLow"};
  const char *argVals[] = {"pump", "led", "fan", "breathe", "rgb", "light", "true", "false",
                           "1", "0", "abc", "", "-5", "255", "256", "999", "4095", "26.5",
                           "50", "60", "1234", "-1"};
  const char serialChars[] = {'9', '0', 'x', '!', '\n', '\r', ' '};
  const int nRoutes = (int)(sizeof(routes) / sizeof(routes[0]));
  const int nNames = (int)(sizeof(argNames) / sizeof(argNames[0]));
  const int nVals = (int)(sizeof(argVals) / sizeof(argVals[0]));

  for (int i = 0; i < iters; i++) {
    switch (rng() % 10) {
      case 0: {  // môi trường ngẫu nhiên
        sys.gWater = (int)(rng() % 4096);
        mockAdc[WATER_PIN] = INVERT_WATER ? 4095 - sys.gWater : sys.gWater;
        sys.gSoil = (int)(rng() % 4096);
        mockAdc[SOIL_PIN] = INVERT_SOIL ? 4095 - sys.gSoil : sys.gSoil;
        sys.gLight = (int)(rng() % 4096);
        mockAdc[LDR_PIN] = INVERT_LIGHT ? 4095 - sys.gLight : sys.gLight;
        sys.gTemp = (float)(rng() % 500) / 10.0f;
        mockDhtTemp = sys.gTemp;
        sys.gHum = (float)(rng() % 101);
        mockDhtHum = sys.gHum;
        mockDigital[TOUCH_PIN] = (rng() % 2) ? HIGH : LOW;
        mockDigital[PIR_PIN] = (rng() % 2) ? HIGH : LOW;
        break;
      }
      case 1: {  // HTTP request ngẫu nhiên với tham số ngẫu nhiên
        int na = (int)(rng() % 4);
        for (int a = 0; a < na; a++) {
          mockServerSetArg(argNames[rng() % nNames], argVals[rng() % nVals]);
        }
        if (WEB_TOKEN[0] != '\0') mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
        bool found = mockServerInvoke(routes[rng() % nRoutes]);
        CHECK(found);
        int code = mockServerLastCode();
#if API_JSON_IS_TINY
        // Buffer JSON cố ý nhỏ -> /api trả 500 thay vì JSON hỏng (vẫn hợp lệ)
        CHECK(code == 200 || code == 204 || code == 400 || code == 500);
#else
        CHECK(code == 200 || code == 204 || code == 400);
#endif
        break;
      }
      case 2:  // input serial ngẫu nhiên
        if (mockSerialPending() < 8) {
          char c[2] = {serialChars[rng() % (int)sizeof(serialChars)], 0};
          mockSerialPush(c);
        }
        break;
      case 3:  // bật/tắt actuator thủ công
        switch (rng() % 4) {
          case 0: setRelay(!sys.relayState); break;
          case 1: setFan(!sys.fanState); break;
          case 2: setBlueLed(!sys.blueLedState); break;
          case 3: setBreatheLed(!sys.breatheLedState); break;
        }
        break;
      case 4:  // cơ cấu servo dùng chung
        switch (rng() % 3) {
          case 0: setWindmill((rng() % 2) != 0); break;
          case 1: startFeed(); break;
          case 2: stopFeed(); break;
        }
        break;
      case 5:
        startDemo();
        break;
      case 6:
        if (WEB_TOKEN[0] != '\0') mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
        mockServerInvoke("/demo");
        break;
      case 7:
        mockDigital[TOUCH_PIN] = (rng() % 2) ? HIGH : LOW;
        mockDigital[PIR_PIN] = (rng() % 2) ? HIGH : LOW;
        break;
      case 8: {  // cập nhật ngưỡng ngẫu nhiên
        char v[16];
        snprintf(v, sizeof(v), "%d", (int)(rng() % 5000) - 100);
        mockServerSetArg(argNames[6 + rng() % 6], v);
        if (WEB_TOKEN[0] != '\0') mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
        mockServerInvoke("/threshold");
        break;
      }
      case 9:
        break;
    }

    mockAdvance(1 + rng() % 250);
    bool demoAtLoopStart = demoRunning();
    loop();
    checkFuzzInvariants(demoAtLoopStart);

    if (i % 1000 == 999) mockSerialClear();   // giới hạn buffer bắt log
  }
}

static void testFuzzSoak() {
  printf("D13 fuzz: seeded soak with safety invariants\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;
  mockServerReset();
  setup();                                   // route + giá trị mặc định từ "flash"

  const int iters = fuzzIterations();
  fuzzRun(1,    iters);
  fuzzRun(42,   iters);
  fuzzRun(2026, iters);

  // ổn định: để demo đang chạy kết thúc, rồi kiểm tra trạng thái yên tĩnh
  stopFeed();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); loop(); }
  CHECK(!demoRunning());
  mockSerialClear();
}

// D14: an toàn wrap-around của millis()
static void testClockWrap() {
  printf("D14 clock: millis() wrap-around\n");
  stopFeed();
  setWindmill(false);

  // A: đọc cảm biến định kỳ xuyên qua mốc wrap
  resetState();
  mockSetMillis(0xFFFFFF00UL);
  mockDhtTemp = 41.0f; mockDhtHum = 33.0f;
  mockAdc[LDR_PIN] = 1500; mockAdc[SOIL_PIN] = 800; mockAdc[WATER_PIN] = 1200;
  sys.lastSensorRead = mockMillis();
  runLoop(5100);                             // vượt mốc wrap tại +256 ms
  CHECK_NEAR(sys.gTemp, 41.0, 0.01);
  CHECK_NEAR(sys.gHum, 33.0, 0.01);

  // B: timer cho ăn trải qua mốc wrap
  mockSetMillis(0xFFFFFFFFUL);
  startFeed();
  CHECK(feedRunning());
  mockAdvance(1);                            // wrap về 0
  mockAdvance(1998);
  controlFeed();
  CHECK(feedRunning());                      // 1999 ms < FEED_DURATION
  mockAdvance(2);
  controlFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  // C: timer breathing trải qua mốc wrap
  mockSetMillis(0xFFFFFFFFUL);
  setBreatheLed(true);
  setLedPwm(0);
  sys.lastBreatheUpdate = mockMillis();
  mockAdvance(BREATHE_INTERVAL - 1);
  controlBreathe();
  CHECK_EQ(currentLedPwm(), 0);
  mockAdvance(1);
  controlBreathe();
  CHECK_EQ(currentLedPwm(), 5);
  setBreatheLed(false);

  // D: chạm bật/tắt với timestamp lớn
  mockSetMillis(0xF0000000UL);
  mockAdvance(10000);
  sys.gTouch = false; controlTouch();
  sys.lastTouch = mockMillis();
  setBlueLed(false);
  bool before = sys.blueLedState;
  mockAdvance(TOUCH_DEBOUNCE);               // xóa cửa sổ debounce trước
  sys.gTouch = true; controlTouch();
  CHECK(sys.blueLedState != before);
  sys.gTouch = false; controlTouch();
  setBlueLed(false);

  // E: demo đầy đủ phải kết thúc sạch khi mốc wrap xảy ra giữa lượt chạy
  mockSetMillis(0xFFFFFF00UL);
  primeSensors(900);
  startDemo();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    runAutomation();
  }
  CHECK(!demoRunning());
  CHECK(!sys.relayState);
  CHECK_EQ(currentLedPwm(), 0);

  // F: timer thử lại WiFi trải qua mốc wrap
  mockSetMillis(0xFFFFFF00UL);
  mockWifiStatus = WL_CONNECTED;
  loop();                                            // xóa cờ đã cảnh báo
  int b0 = mockWifiBeginCalls;
  mockWifiStatus = WL_DISCONNECTED;
  sys.lastWifiCheck = 0xFFFFFF00UL;
  mockAdvance(9999);                                  // wrap qua mốc biên
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                   // 9999 ms: chưa đến hạn
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);               // 10000 ms: đến hạn
  mockWifiStatus = WL_CONNECTED;
  loop();

  resetState();
}

// T16: quyền sở hữu LED cảnh báo của PIR + khôi phục breathing (hoạt động 3/5)
static void testAlertLedOwnership() {
  printf("T16 alerts: PIR LED ownership + breathing restore\n");

  // Có chuyển động khi đang breathing: cảnh báo chiếm LED; breathing tiếp tục sau đó
  resetState();
  sys.gPir = false; controlAlerts();          // xóa mọi trạng thái cảnh báo đang chờ
  setBreatheAuto(true);
  setBreatheLed(true);
  CHECK(sys.breatheLedState);
  sys.gPir = true;
  controlAlerts();
  CHECK(sys.blueLedState);
  CHECK(!sys.breatheLedState);                // cảnh báo đã chiếm LED
  sys.gPir = false;
  controlAlerts();
  CHECK(!sys.blueLedState);
  CHECK(sys.breatheLedState);                 // breathing tự chọn được bật lại
  CHECK(sys.breatheAuto);
  setBreatheAuto(false);
  setBreatheLed(false);

  // LED bật thủ công không bị ảnh hưởng khi có người đi qua (PIR không sở hữu nó)
  resetState();
  sys.gPir = false; controlAlerts();
  setBlueLed(true);
  sys.gPir = true;  controlAlerts();
  CHECK(sys.blueLedState);
  sys.gPir = false; controlAlerts();
  CHECK(sys.blueLedState);
  setBlueLed(false);

  // Không có breathing auto, cảnh báo chỉ tắt LED trở lại
  resetState();
  sys.gPir = false; controlAlerts();
  sys.gPir = true;  controlAlerts();
  CHECK(sys.blueLedState);
  sys.gPir = false; controlAlerts();
  CHECK(!sys.blueLedState);
  CHECK(!sys.breatheLedState);

  // Phòng thủ: giá trị ramp cũ được xóa khi breathing tắt
  resetState();
  sys.breatheBrightness = 100;
  setLedPwm(222);
  controlBreathe();
  CHECK_EQ(sys.breatheBrightness, 0);
  CHECK_EQ(currentLedPwm(), 0);
}

// T17: demo hoạt động khôi phục breathing tự chọn khi kết thúc
static void testDemoBreathingRestore() {
  printf("T17 demo: breathing restored after the show\n");
  resetState();
  primeSensors(900);
  setBreatheAuto(true);
  setBreatheLed(true);
  startDemo();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); runAutomation(); }
  CHECK(!demoRunning());
  CHECK(sys.breatheAuto);
  CHECK(sys.breatheLedState);                 // breathing bật lại sau DEMO DONE
  CHECK(!sys.blueLedState);
  setBreatheAuto(false);
  setBreatheLed(false);
}

// D15: chặn CSRF — trang web khác không được điều khiển thiết bị
static void testWebSecurity() {
  printf("D15 http: CSRF guard (Origin/Referer/Sec-Fetch-Site)\n");
  mockServerReset();
  setup();

  const String me = String("http://") + String(mockWifiIp);

  // Yêu cầu không kèm header (curl, app tự viết) vẫn được phép
  bool before = sys.relayState;
  CHECK_EQ(invoke("/toggle", "id", "pump"), 200);
  CHECK(sys.relayState != before);

  // Origin lạ (trang web khác gọi fetch/XHR) → 403, trạng thái không đổi
  before = sys.relayState;
  mockServerSetHeader("Origin", "http://evil.example");
  CHECK_EQ(invoke("/toggle", "id", "pump"), 403);
  CHECK(sys.relayState == before);
  CHECK_STR(mockServerLastBody(), "forbidden");

  // Referer lạ (thẻ <img> không gửi Origin) → 403
  mockServerSetHeader("Referer", "http://evil.example/attack.html");
  CHECK_EQ(invoke("/feed"), 403);
  CHECK(!feedRunning());

  // Sec-Fetch-Site: cross-site → 403 (trình duyệt hiện đại)
  mockServerSetHeader("Sec-Fetch-Site", "cross-site");
  CHECK_EQ(invoke("/demo"), 403);
  CHECK(!demoRunning());

  // Tên miền giả mạo kiểu "http://<ip>.evil.com" không được lọt qua
  mockServerSetHeader("Origin", (me + ".evil.com").c_str());
  CHECK_EQ(invoke("/toggle", "id", "fan"), 403);

  // Yêu cầu cùng nguồn (từ chính trang của thiết bị) được phép
  mockServerSetHeader("Origin", me.c_str());
  mockServerSetHeader("Referer", (me + "/").c_str());
  mockServerSetHeader("Sec-Fetch-Site", "same-origin");
  CHECK_EQ(invoke("/windmill", "on", "true"), 200);
  CHECK(sys.windmillOn);

  // Request cùng nguồn lần hai dùng lại tiền tố origin đã nhớ (không dựng lại)
  mockServerSetHeader("Origin", me.c_str());
  mockServerSetHeader("Sec-Fetch-Site", "same-origin");
  CHECK_EQ(invoke("/windmill", "on", "false"), 200);
  CHECK(!sys.windmillOn);

  // DHCP cấp IP mới → tiền tố origin phải được dựng lại theo IP mới
  const char *savedIp = mockWifiIp;
  mockWifiIp = "10.0.0.7";
  mockServerSetHeader("Origin", "http://10.0.0.7");
  CHECK_EQ(invoke("/windmill", "on", "true"), 200);
  CHECK(sys.windmillOn);
  mockWifiIp = savedIp;

  resetState();
}

// D16: token web tùy chọn (chỉ chạy ở bản build có đặt WEB_TOKEN)
static void testWebToken() {
  printf("D16 http: optional WEB_TOKEN guard\n");
  if (WEB_TOKEN[0] == '\0') {
    printf("      (bỏ qua: bản build này không đặt WEB_TOKEN)\n");
    return;
  }
  mockServerReset();
  setup();

  // Không kèm token → 401, trạng thái không đổi
  bool before = sys.relayState;
  mockServerSetArg("id", "pump");
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 401);
  CHECK_STR(mockServerLastBody(), "unauthorized");
  CHECK(sys.relayState == before);

  // Token sai qua query → 401
  mockServerSetArg("id", "pump");
  mockServerSetArg("token", "sai-token");
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 401);
  CHECK(sys.relayState == before);

  // Token đúng qua query → 200
  mockServerSetArg("id", "pump");
  mockServerSetArg("token", WEB_TOKEN);
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 200);
  CHECK(sys.relayState != before);

  // Token đúng qua header X-Auth-Token → 200
  before = sys.relayState;
  mockServerSetArg("id", "pump");
  mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 200);
  CHECK(sys.relayState != before);

  // /api cũng yêu cầu token (trả dữ liệu cảm biến + nonce)
  CHECK(mockServerInvoke("/api"));            // không kèm token → 401
  CHECK_EQ(mockServerLastCode(), 401);
  mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
  CHECK(mockServerInvoke("/api"));            // kèm token → 200
#if API_JSON_IS_TINY
  CHECK_EQ(mockServerLastCode(), 500);        // buffer JSON nhỏ -> báo lỗi
#else
  CHECK_EQ(mockServerLastCode(), 200);
#endif

  resetState();
}

// D18: bắt đầu trình diễn phải đóng cửa máng ăn đang mở
static void testDemoClosesFeed() {
  printf("D18 demo: starting the show closes an open feed gate\n");
  resetState();
  primeSensors(900);
  startFeed();
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);

  startDemo();
  CHECK(!feedRunning());                     // demo tiếp quản servo
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);  // cửa phải đóng ngay
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); runAutomation(); }
  CHECK(!demoRunning());
  resetState();
}

// D19: vòng loop đầu tiên đọc cảm biến TRƯỚC khi automation chạy
static void testBootFirstSample() {
  printf("D19 boot: first loop samples sensors before automation\n");
  resetState();
  mockServerReset();
  mockWifiStatus = WL_CONNECTED;             // boot nhanh: millis < 1000
  mockDhtTemp = 33.5f; mockDhtHum = 44.0f;
  mockAdc[LDR_PIN] = 3000;                   // sáng -> không bật đèn
  mockAdc[SOIL_PIN] = 900;                   // đất ẩm -> không bật bơm
  mockAdc[WATER_PIN] = INVERT_WATER ? 2095 : 2000;  // đủ nước -> không hú còi

  setup();
  CHECK(mockMillis() < SENSOR_INTERVAL);     // đúng kịch bản: chưa đủ 1 giây
  mockSerialClear();
  loop();
  CHECK_NEAR(sys.gTemp, 33.5, 0.05);         // đã đọc cảm biến ngay vòng đầu
  CHECK_NEAR(sys.gHum, 44.0, 0.05);
  checkBuzzer(false);                        // không hú còi oan với nước "0"
  CHECK(!sys.blueLedState);                  // không chớp đèn vì trời "tối"
  CHECK(!sys.relayState);
  resetState();
}

// D17: tham số ngưỡng không hữu hạn ("nan"/"inf") không được lọt vào state
static void testThresholdNonFinite() {
  printf("D17 http: non-finite threshold args sanitized\n");
  resetState();
  mockServerReset();
  setup();

  CHECK_EQ(invoke("/threshold", "soilLow", "nan"), 200);
  CHECK(isfinite(sys.soilThresholdLow));
  CHECK_NEAR(sys.soilThresholdLow, 0, 0.01);   // quy về 0, giống toFloat("abc")
  CHECK(sys.soilThresholdHigh > sys.soilThresholdLow);

  invoke("/threshold", "soilHigh", "inf");
  CHECK(isfinite(sys.soilThresholdHigh));
  CHECK(sys.soilThresholdHigh > sys.soilThresholdLow);

  invoke("/threshold", "waterLow", "-inf");
  CHECK(isfinite(sys.waterThresholdLow));
  CHECK_NEAR(sys.waterThresholdLow, 0, 0.01);

  invoke("/threshold", "tempHigh", "nan");
  invoke("/threshold", "tempLow", "inf");
  CHECK(isfinite(sys.tempThresholdHigh));
  CHECK(isfinite(sys.tempThresholdLow));
  CHECK(sys.tempThresholdHigh > sys.tempThresholdLow);   // tự sửa thứ tự ngưỡng

  // Cảm biến hữu hạn bình thường: logic bơm không bị kẹt vì NaN
  sys.soilThresholdLow = 400; sys.soilThresholdHigh = 700;
  sys.gWater = 900; sys.gSoil = 100; sys.pumpAuto = true;
  controlPump();
  CHECK(sys.relayState);
  setRelay(false);
  resetState();
}

// I11: boot suy giảm - thiếu OLED và timeout kết nối WiFi (setup KHÔNG chặn)
static void testBootDegraded() {
  printf("I11 boot: missing OLED + non-blocking WiFi timeout\n");
  resetState();
  mockServerReset();
  mockDisplayBeginOk = false;
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  setup();

  checkAllRoutes();                                  // route vẫn được đăng ký
  CHECK(has(mockSerialOutput(), "MA LOI: 101"));     // không tìm thấy OLED
  CHECK(!has(mockSerialOutput(), "MA LOI: 102"));    // setup trả về ngay, chưa timeout
  CHECK(mockWifiBeginCalls >= 1);
  CHECK(!demoRunning());

  // Timeout WiFi giờ báo nền trong loop() (không chặn boot), watchdog vẫn được nuôi
  mockSerialClear();
  primeSensors(900);
  runLoop(WIFI_CONNECT_TIMEOUT + 1000);
  CHECK(has(mockSerialOutput(), "MA LOI: 102"));
  CHECK(mockWdtResetCalls > 0);
  CHECK(mockWifiBeginCalls >= 2);                    // ít nhất một lần thử lại trong nền

  mockDisplayBeginOk = true;
  mockWifiStatus = WL_CONNECTED;
  resetState();
}

// D20: các nhánh biên còn sót mà các test trước chưa chạm tới:
// CSRF cùng nguồn có cổng/query, id lạ, ngưỡng tự chỉnh, /wifi không đổi
// SSID, nonce tràn 32 bit, OTA lỗi lạ, log bị chặn dưới ngưỡng, WiFi rớt.
static void testEdgeBranches() {
  printf("D22 edge: remaining boundary branches\n");
  resetState();
  mockServerReset();
  mockWifiStatus = WL_CONNECTED;
  setup();

  const String me = String("http://") + String(mockWifiIp);

  // ── CSRF: cùng nguồn nhưng có cổng hoặc query vẫn được phép ──
  mockServerSetHeader("Referer", (me + ":8080/trang").c_str());
  CHECK_EQ(invoke("/windmill", "on", "true"), 200);
  CHECK(sys.windmillOn);

  mockServerSetHeader("Referer", (me + "?v=2").c_str());
  CHECK_EQ(invoke("/windmill", "on", "false"), 200);
  CHECK(!sys.windmillOn);

  // ── id không hợp lệ → 400, trạng thái không đổi ──
  mockServerSetHeader("Referer", "");
  bool before = sys.relayState;
  CHECK_EQ(invoke("/toggle", "id", "khong-co"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");
  CHECK(sys.relayState == before);
  CHECK_EQ(invoke("/auto", "id", "khong-co"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");

  // ── /threshold và /wifi không có tham số → 400 ──
  CHECK_EQ(invoke("/threshold"), 400);
  CHECK_STR(mockServerLastBody(), "no params");
  CHECK_EQ(invoke("/wifi"), 400);
  CHECK_STR(mockServerLastBody(), "no params");

  // ── ngưỡng bị gửi lộn xộn → tự chỉnh để high > low ──
  CHECK_EQ(invoke("/threshold", "soilLow", "100", "soilHigh", "50"), 200);
  CHECK_NEAR(sys.soilThresholdLow, 100.0, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 150.0, 0.01);
  CHECK_EQ(invoke("/threshold", "tempLow", "40", "tempHigh", "30"), 200);
  CHECK_NEAR(sys.tempThresholdLow, 40.0, 0.01);
  CHECK_NEAR(sys.tempThresholdHigh, 41.0, 0.01);

  // ── chỉ đổi mật khẩu WiFi (giữ nguyên SSID) → vẫn phải ghi flash ──
  CHECK_EQ(invoke("/wifi", "password", "mat-khau-moi"), 200);
  CHECK_STR(sys.password, "mat-khau-moi");
  CHECK(sys.gNeedRestart);
  sys.gNeedRestart = false;

  // ── token dài hơn giá trị thật → 401 (so sánh constant-time phải thấy cả độ dài) ──
  if (WEB_TOKEN[0] != '\0') {
    std::string longTok(WEB_TOKEN);
    longTok += "-bi-sai-do-dai";
    mockServerSetHeader("X-Auth-Token", longTok.c_str());
    mockServerSetArg("id", "pump");
    CHECK(mockServerInvoke("/toggle"));
    CHECK_EQ(mockServerLastCode(), 401);
  }

  // ── nonce tràn 32 bit → /api trả nonce hợp lệ, không trả 0 ──
  uint32_t savedNonce = sys.gLastNonce;
  sys.gLastNonce = 0xFFFFFFFFul;
#if API_JSON_IS_TINY
  CHECK_EQ(invoke("/api"), 500);        // buffer JSON nhỏ -> báo lỗi, không có body JSON
#else
  CHECK_EQ(invoke("/api"), 200);
  CHECK(has(mockServerLastBody(), "\"nonce\":1"));
#endif
  sys.gLastNonce = savedNonce;

  // ── OTA: total = 0 (ảnh firmware rỗng) không được chia cho 0 ──
  mockSerialClear();
  mockOtaProgress(10, 0);
  CHECK(has(mockSerialOutput(), "OTA: 0%"));

  // ── OTA: mã lỗi lạ → in mã lỗi, không treo ──
  mockSerialClear();
  mockOtaError(99);
  CHECK(has(mockSerialOutput(), "OTA: error[99]"));

  // ── log lỗi ở mức thấp hơn ngưỡng tối thiểu → không in ──
  mockSerialClear();
  gMinLogLevel = LOG_FATAL;
  LOG_ERROR_CODE(777, "loi bi chan");
  CHECK(!has(mockSerialOutput(), "loi bi chan"));
  gMinLogLevel = LOG_INFO;

  // ── debug webserver khi WiFi rớt → in FAIL ──
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  debugWebserver();
  CHECK(has(mockSerialOutput(), "FAIL"));
  mockWifiStatus = WL_CONNECTED;

  // ── trình tự debug cơ cấu: gọi update khi bước chưa hết hạn → không nhảy bước ──
  mockSerialClear();
  debugActuators();
  updateDebugActuators();
  CHECK(sys.relayState);                       // bước 0 (bơm) vẫn đang chạy
  stopDebugActuators();
  CHECK(!sys.relayState);

  resetState();
}

// D21: mọi route có bảo vệ đều phải chặn được nguồn lạ; cờ debug in đúng
// chỗ; nhánh phòng vệ của cơ cấu chấp hành.
static void testGuardAndDebugFlags() {
  printf("D23 http: guarded routes + module debug flags\n");
  resetState();
  mockServerReset();
  mockWifiStatus = WL_CONNECTED;
  setup();

  // ── mọi route có guardWeb() đều phải trả 403 cho yêu cầu từ nguồn lạ ──
  static const char *guarded[] = {
    "/toggle", "/auto", "/rgb", "/breathe", "/on", "/off", "/servo",
    "/windmill", "/feed", "/demo", "/threshold", "/wifi"
  };
  for (const char *path : guarded) {
    mockServerSetHeader("Origin", "http://evil.example");
    CHECK(mockServerInvoke(path));
    CHECK_EQ(mockServerLastCode(), 403);
  }
  CHECK(!demoRunning());
  CHECK(!feedRunning());
  CHECK(!sys.relayState);

  // ── cờ debug bật: MOD_DEBUG phải in ra, không im lặng ──
  dbgSensors = dbgActuators = dbgControl = dbgWebserver = true;
  dbgWifi = dbgOled = dbgSettings = dbgLogger = dbgRgb = true;
  gMinLogLevel = LOG_DEBUG;

  mockSerialClear();
  CHECK_EQ(invoke("/"), 200);
  CHECK(has(mockSerialOutput(), "GET /"));

  mockSerialClear();
  CHECK_EQ(invoke("/api"), API_JSON_IS_TINY ? 500 : 200);
  CHECK(has(mockSerialOutput(), "GET /api"));

  mockSerialClear();
  CHECK_EQ(invoke("/on"), 200);
  CHECK(has(mockSerialOutput(), "GET /on"));

  mockSerialClear();
  CHECK_EQ(invoke("/off"), 200);
  CHECK(has(mockSerialOutput(), "GET /off"));

  mockSerialClear();
  CHECK_EQ(invoke("/demo"), 200);
  CHECK(has(mockSerialOutput(), "GET /demo"));

  // MOD_DEBUG(Rgb) trong actuators.cpp
  mockSerialClear();
  setRgb(1, 2, 3);
  CHECK(has(mockSerialOutput(), "RGB set"));
  mockSerialClear();
  setRgbPlant();
  CHECK(has(mockSerialOutput(), "RGB plant light"));

  // Các hàm debug in đủ nhãn
  mockSerialClear();
  debugSensors();   debugActuators(); debugControl();
  debugWebserver(); debugWifi();      debugOled();
  debugSettings();  debugLogger();
  CHECK(has(mockSerialOutput(), "--- SENSORS ---"));
  CHECK(has(mockSerialOutput(), "--- ACTUATORS ---"));
  CHECK(has(mockSerialOutput(), "--- CONTROL ---"));
  CHECK(has(mockSerialOutput(), "--- WEBSERVER ---"));
  CHECK(has(mockSerialOutput(), "--- WIFI ---"));
  CHECK(has(mockSerialOutput(), "--- OLED ---"));
  CHECK(has(mockSerialOutput(), "--- SETTINGS ---"));
  CHECK(has(mockSerialOutput(), "--- LOGGER ---"));

  // ── breatheAuto = true → /api báo đúng, không báo "false" ──
  sys.breatheAuto = true;
  CHECK_EQ(invoke("/api"), API_JSON_IS_TINY ? 500 : 200);
#if !API_JSON_IS_TINY
  CHECK(has(mockServerLastBody(), "\"breatheAuto\":true"));
#endif

  // ── nhánh phòng vệ: đèn sáng liên tục phải thắng chế độ thở ──
  sys.blueLedState = true;
  sys.breatheLedState = true;
  setBlueLed(true);
  CHECK(sys.blueLedState);
  CHECK(!sys.breatheLedState);

  // dọn cờ debug + mức log
  dbgSensors = dbgActuators = dbgControl = dbgWebserver = false;
  dbgWifi = dbgOled = dbgSettings = dbgLogger = dbgRgb = false;
  gMinLogLevel = LOG_INFO;

  resetState();
}

// D24: các nhánh phòng vệ của logic điều khiển, cảm biến, cài đặt và OLED:
// clamp ngưỡng khi flash hỏng, RGB bật/tắt từng phần, PIR giữ đèn, DHT trả
// NaN khi bộ đệm còn trống, và cờ "không chạy" của trình diễn.
static void testDefensiveBranches() {
  printf("D24 defensive: clamp + ownership + sensor fallback\n");
  resetState();

  // ── flash lưu ngưỡng lỗi / ngoài khoảng → phải clamp về khoảng hợp lệ ──
  mockPrefPut("soilLow", -100.0f);
  mockPrefPut("soilHigh", 99999.0f);
  mockPrefPut("waterLow", -50.0f);
  mockPrefPut("lightThr", -100.0f);
  mockPrefPut("tempHigh", -10.0f);
  mockPrefPut("tempLow", 99999.0f);
  loadSettings();
  CHECK_NEAR(sys.soilThresholdLow, 0.0, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 4095.0, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 0.0, 0.01);
  CHECK_EQ(sys.lightThreshold, 0);
  CHECK_NEAR(sys.tempThresholdHigh, 0.0, 0.01);
  CHECK_NEAR(sys.tempThresholdLow, 60.0, 0.01);

  // Ngưỡng vượt trần cũng phải bị clamp về 4095
  mockPrefPut("lightThr", 99999.0f);
  loadSettings();
  CHECK_EQ(sys.lightThreshold, 4095);

  // Ngưỡng flash hợp lệ → giữ nguyên
  mockPrefPut("soilLow", 300.0f);
  mockPrefPut("soilHigh", 800.0f);
  mockPrefPut("waterLow", 250.0f);
  mockPrefPut("lightThr", 500);
  mockPrefPut("tempLow", 20.0f);
  mockPrefPut("tempHigh", 35.0f);
  loadSettings();
  CHECK_NEAR(sys.soilThresholdLow, 300.0, 0.01);
  CHECK_NEAR(sys.soilThresholdHigh, 800.0, 0.01);
  CHECK_NEAR(sys.waterThresholdLow, 250.0, 0.01);
  CHECK_EQ(sys.lightThreshold, 500);
  CHECK_NEAR(sys.tempThresholdLow, 20.0, 0.01);
  CHECK_NEAR(sys.tempThresholdHigh, 35.0, 0.01);

  // ── ngưỡng web vượt trần → constrain về 4095 ──
  resetState();
  mockServerReset();
  setup();
  CHECK_EQ(invoke("/threshold", "soilLow", "4090", "soilHigh", "4090"), 200);
  CHECK_NEAR(sys.soilThresholdHigh, 4095.0, 0.01);

  // ── RGB: chỉ G hoặc chỉ B khác 0 thì vẫn coi là "đang tắt" ──
  sys.rgbAuto = true;
  sys.gLight = 3000;                                  // sáng -> tắt RGB
  sys.rgbR = 0; sys.rgbG = 7; sys.rgbB = 0;
  controlRgb();
  CHECK_EQ(sys.rgbR, 0);
  CHECK_EQ(sys.rgbG, 0);
  CHECK_EQ(sys.rgbB, 0);

  sys.rgbR = 0; sys.rgbG = 0; sys.rgbB = 7;
  controlRgb();
  CHECK_EQ(sys.rgbB, 0);

  // ── PIR giữ đèn: auto bật nhưng có người thì không tự điều khiển ──
  sys.lightAuto = true;
  sys.breatheLedState = false;
  sys.gPir = true;
  sys.gLight = 0;                                     // rất tối -> lẽ ra phải bật đèn
  sys.blueLedState = false;
  controlLight();
  CHECK(!sys.blueLedState);                           // nhưng PIR đang giữ -> không bật

  sys.gPir = false;
  controlLight();
  CHECK(sys.blueLedState);                            // không còn PIR -> bật đèn theo ánh sáng

  // Đèn thở đang giữ LED -> auto đèn không được tranh giành
  sys.lightAuto = true;
  sys.breatheLedState = true;
  sys.gPir = false;
  sys.gLight = 0;
  sys.blueLedState = false;
  controlLight();
  CHECK(!sys.blueLedState);
  sys.breatheLedState = false;

  // ── DHT trả NaN khi bộ đệm còn trống → giá trị thay thế là 0 ──
  resetState();
  mockDhtTemp = NAN;
  mockDhtHum = NAN;
  readAllSensors();
  CHECK_NEAR(sys.gTemp, 0.0, 0.01);
  CHECK_NEAR(sys.gHum, 0.0, 0.01);

  // ── updateDemo() khi không có demo chạy → thoát ngay, không làm gì ──
  CHECK(!demoRunning());
  updateDemo();
  CHECK(!demoRunning());

  resetState();
}

// ==============================================
//  Hàm chính
// ==============================================

int main() {
  printf("=== Smart Farm deep test suite (core %d, invert L/S/W %d/%d/%d, relayLO %d, buzzPassive %d) ===\n",
         ESP_ARDUINO_VERSION_MAJOR, INVERT_LIGHT, INVERT_SOIL, INVERT_WATER,
         RELAY_ACTIVE_LOW, BUZZER_PASSIVE);

  RUN_TEST(testSensorsAverage);
  RUN_TEST(testDhtFailover);
  RUN_TEST(testPumpHysteresis);
  RUN_TEST(testPumpLowWaterSafety);
  RUN_TEST(testFanHysteresis);
  RUN_TEST(testLightPirAlerts);
  RUN_TEST(testBreatheAndLedModes);
  RUN_TEST(testTouchToggle);
  RUN_TEST(testRgbPlant);
  RUN_TEST(testBuzzerAlarm);
  RUN_TEST(testWindmillSweep);
  RUN_TEST(testFeedGate);
  RUN_TEST(testWeather);
  RUN_TEST(testSettingsRoundtrip);
  RUN_TEST(testSettingsResetAndSkip);
  RUN_TEST(testLoggerLevelGuard);
  RUN_TEST(testSettingsNonFiniteFallback);
  RUN_TEST(testDemoFullRun);
  RUN_TEST(testAlertLedOwnership);
  RUN_TEST(testDemoBreathingRestore);

  RUN_TEST(testBoundaries);
  RUN_TEST(testIdempotency);
  RUN_TEST(testTimingBoundaries);

  printf("--- integration ---\n");
  RUN_TEST(testInoBoot);
  RUN_TEST(testHttpApi);
  RUN_TEST(testHttpLedOnOff);
  RUN_TEST(testHttpMechanisms);
  RUN_TEST(testHttpControlRoutes);
  RUN_TEST(testHttpRootFavicon);
  RUN_TEST(testHttpWifiConfig);
  RUN_TEST(testOtaCallbacks);
  RUN_TEST(testOled);
  RUN_TEST(testSerialMenu);
  RUN_TEST(testSerialDemo);
  RUN_TEST(testSerialActuatorSequence);
  RUN_TEST(testWifiRecovery);

  printf("--- deep ---\n");
  RUN_TEST(testApiJsonContract);
  RUN_TEST(testApiJsonTruncation);
  RUN_TEST(testResponseExactness);
  RUN_TEST(testOledLines);
  RUN_TEST(testOledDelta);
  RUN_TEST(testOledAbsentOrBroken);
  RUN_TEST(testOledDirtyRefresh);
  RUN_TEST(testSettingsRangeClamp);
  RUN_TEST(testSerialEdges);
  RUN_TEST(testDemoDiagnostics);
  RUN_TEST(testWifiThrottle);
  RUN_TEST(testWifiStatusPoll);
  RUN_TEST(testOtaHandleThrottle);
  RUN_TEST(testBootOrder);
  RUN_TEST(testRebootPersistence);
  RUN_TEST(testFuzzSoak);
  RUN_TEST(testClockWrap);
  RUN_TEST(testWebSecurity);
  RUN_TEST(testWebToken);
  RUN_TEST(testThresholdNonFinite);
  RUN_TEST(testDemoClosesFeed);
  RUN_TEST(testBootFirstSample);
  RUN_TEST(testBootDegraded);
  RUN_TEST(testEdgeBranches);
  RUN_TEST(testGuardAndDebugFlags);
  RUN_TEST(testDefensiveBranches);

  printf("=== %d checks, %d failures ===\n", gChecks, gFails);
  return gFails ? 1 : 0;
}
