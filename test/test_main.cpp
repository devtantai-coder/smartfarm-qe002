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
// Cách build & chạy:  ./test/run_tests.sh   (xem test/README.md)
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <random>

#include "mock.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"
#include "control.h"
#include "demo.h"
#include "settings.h"
#include "oled.h"
#include "config.h"

// Các entry point thật của firmware (SmartFarm.ino)
void setup();
void loop();

static int gChecks = 0, gFails = 0;

#define CHECK(cond) do { \
  gChecks++; \
  if (!(cond)) { gFails++; printf("  FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

#define CHECK_EQ(a, b) do { \
  gChecks++; \
  if ((long)(a) != (long)(b)) { \
    gFails++; printf("  FAIL line %d: %s (%ld) != %s (%ld)\n", \
                     __LINE__, #a, (long)(a), #b, (long)(b)); \
  } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
  gChecks++; \
  if (fabs((double)(a) - (double)(b)) > (eps)) { \
    gFails++; printf("  FAIL line %d: %s (%.2f) != %s (%.2f)\n", \
                     __LINE__, #a, (double)(a), #b, (double)(b)); \
  } \
} while (0)

#define CHECK_STR(a, b) do { \
  gChecks++; \
  if (strcmp((a), (b)) != 0) { \
    gFails++; printf("  FAIL line %d: \"%s\" != \"%s\"\n", __LINE__, (a), (b)); \
  } \
} while (0)

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
void testResetWifiStatics();
#endif

static void resetState() {
  mockReset();
#ifdef SMARTFARM_TEST_HOOKS
  testResetActuatorStatics();
  testResetControlStatics();
  testResetDemoStatics();
  testResetSensorStatics();
  testResetWifiStatics();
#endif
  soilThresholdLow = 400; soilThresholdHigh = 700; waterThresholdLow = 200;
  lightThreshold = 500; tempThresholdHigh = 30; tempThresholdLow = 25;
  pumpAuto = lightAuto = rgbAuto = fanAuto = true;
  breatheAuto = false;
  relayState = fanState = blueLedState = breatheLedState = false;
  windmillOn = false;
  rgbR = rgbG = rgbB = 0;
  breatheBrightness = 0;
  breatheDirection = 1;
  servoAngle = 0;
  gTemp = 22; gHum = 50; gLight = 300; gSoil = 500; gWater = 800;
  gPir = false; gTouch = false;
  lastWifiCheck = lastSensorRead = lastOledUpdate = 0;
  lastBuzzer = lastTouch = lastBreatheUpdate = 0;
  sensorFailCount = 0;
  sensorHealthy = true;
}

// Mô phỏng đúng phần điều khiển trong loop() của SmartFarm.ino (không có HTTP/OLED)
static void simLoop() {
  unsigned long now = mockMillis();
  gTouch = readTouch();
  gPir = readPIR();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    lastSensorRead = now;
  }
  if (gWater < (int)waterThresholdLow && relayState) setRelay(false);
  if (demoRunning()) {
    updateDemo();
    controlBreathe();
  } else {
    controlPump(); controlFan(); controlLight(); controlRgb(); controlBreathe();
    controlTouch(); controlAlerts(); controlWindmill(); controlFeed();
  }
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
  lastSensorRead = mockMillis();
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
  CHECK_EQ(gLight, dir(3000, INVERT_LIGHT));
  CHECK_EQ(gSoil, dir(600, INVERT_SOIL));
  CHECK_EQ(gWater, dir(900, INVERT_WATER));
  CHECK_NEAR(gTemp, 26.5, 0.05);
  CHECK_NEAR(gHum, 61.0, 0.05);

  mockAdc[LDR_PIN] = 2000;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_EQ(gLight, dir(2000, INVERT_LIGHT));

  mockAdc[LDR_PIN] = 1000;
  readAllSensors();
  int expected = (4 * dir(2000, INVERT_LIGHT) + dir(1000, INVERT_LIGHT)) / 5;
  CHECK_EQ(gLight, expected);
}

// T2: DHT NaN failover + cờ health
static void testDhtFailover() {
  printf("T2  sensors: DHT11 NaN failover\n");
  resetState();
  mockDhtTemp = 26.0f; mockDhtHum = 55.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_NEAR(gTemp, 26.0, 0.05);

  mockDhtTemp = NAN;
  readAllSensors();
  CHECK_NEAR(gTemp, 26.0, 0.05);
  for (int i = 0; i < 6; i++) readAllSensors();
  CHECK(!sensorHealthy);

  mockDhtTemp = 24.0f; mockDhtHum = 50.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK(sensorHealthy);
  CHECK_NEAR(gTemp, 24.0, 0.05);

  // Độ ẩm NaN dùng lại giá trị lọc tốt cuối cùng (giống nhiệt độ)
  mockDhtTemp = 27.0f; mockDhtHum = NAN;
  readAllSensors();
  CHECK_NEAR(gHum, 50.0, 0.5);
  CHECK(sensorHealthy);        // một NaN độ ẩm đơn lẻ không đánh dấu cảm biến lỗi
  mockDhtHum = 50.0f;
  readAllSensors();
}

// T3: trễ bật/tắt bơm theo độ ẩm đất (hoạt động 7/10)
static void testPumpHysteresis() {
  printf("T3  pump: soil hysteresis\n");
  resetState();
  gWater = 900;
  gSoil = 300;
  controlPump();
  CHECK(relayState);
  checkRelayPin(true);
  gSoil = 500;                 // giữa các ngưỡng: không đổi
  controlPump();
  CHECK(relayState);
  gSoil = 800;
  controlPump();
  CHECK(!relayState);
  checkRelayPin(false);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  soilThresholdLow = 700; soilThresholdHigh = 400;
  gSoil = 500; gWater = 900;
  controlPump();
  CHECK(!relayState);
  controlPump();
  CHECK(!relayState);
}

// T4: an toàn hết nước của bơm kể cả ở chế độ thủ công (hoạt động 8)
static void testPumpLowWaterSafety() {
  printf("T4  pump: low-water safety\n");
  resetState();
  pumpAuto = false;
  setRelay(true);
  CHECK(relayState);
  gWater = 100;
  controlPump();
  CHECK(!relayState);

  pumpAuto = true;
  gSoil = 100;                 // rất khô nhưng không có nước
  gWater = 100;
  controlPump();
  CHECK(!relayState);
}

// T5: trễ bật/tắt quạt theo nhiệt độ
static void testFanHysteresis() {
  printf("T5  fan: temperature hysteresis\n");
  resetState();
  gTemp = 31; controlFan(); CHECK(fanState);
  gTemp = 27; controlFan(); CHECK(fanState);
  gTemp = 24; controlFan(); CHECK(!fanState);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  tempThresholdHigh = 25; tempThresholdLow = 30;
  gTemp = 27;
  controlFan();
  CHECK(!fanState);
  controlFan();
  CHECK(!fanState);
}

// T6: đèn auto + PIR chiếm quyền + cảnh báo
static void testLightPirAlerts() {
  printf("T6  light auto / PIR / alerts\n");
  resetState();
  gLight = 400;                // tối
  controlLight();
  CHECK(blueLedState);
  gLight = 600;                // sáng
  controlLight();
  CHECK(!blueLedState);

  gPir = true;                 // PIR nắm LED: đèn auto phải nhường
  gLight = 400;
  controlLight();
  CHECK(!blueLedState);

  controlAlerts();             // PIR bật đèn
  CHECK(blueLedState);
  gPir = false;
  controlAlerts();             // và tắt khi hết chuyển động
  CHECK(!blueLedState);
}

// T7: LED xanh sáng đều vs breathing (hoạt động 3) + điều phối PWM
static void testBreatheAndLedModes() {
  printf("T7  LED: steady/breathe modes + PWM dispatch\n");
  resetState();
  setBreatheLed(true);
  CHECK(breatheLedState);
  CHECK(!blueLedState);
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
  CHECK(!breatheLedState);
  CHECK(blueLedState);
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
  gTouch = true;
  controlTouch();
  CHECK(blueLedState);
  controlTouch();              // giữ: không lặp lại
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();
  gTouch = true;
  mockAdvance(TOUCH_DEBOUNCE - 100);
  controlTouch();              // trong thời gian debounce: lần nhấn bị nuốt
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();              // nhả
  mockAdvance(200);            // lúc này đã qua cửa sổ debounce
  gTouch = true;
  controlTouch();              // tắt
  CHECK(!blueLedState);
  CHECK(!lightAuto);           // thao tác thủ công đã tắt auto
}

// T9: đèn RGB trồng cây (hoạt động 9)
static void testRgbPlant() {
  printf("T9  RGB: plant pattern (activity 9)\n");
  resetState();
  setRgb(10, 20, 30);
  CHECK_EQ(mockPixel[3], (10 << 16) | (20 << 8) | 30);

  gLight = 400;                // tối -> kiểu đèn cho cây
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
  CHECK_EQ(mockPixel[1], 0x0000FF);
  CHECK_EQ(mockPixel[2], 0xFF0000);
  CHECK_EQ(mockPixel[3], 0x0000FF);
  CHECK_EQ(rgbR, 255); CHECK_EQ(rgbG, 0); CHECK_EQ(rgbB, 255);

  gLight = 600;                // sáng -> tắt
  controlRgb();
  for (int i = 0; i < RGB_COUNT; i++) CHECK_EQ(mockPixel[i], 0);
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbB, 0);

  setRgb(10, 20, 30);          // màu thủ công, rồi tối lại
  gLight = 400;
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
}

// T10: còi báo động hết nước (hoạt động 8)
static void testBuzzerAlarm() {
  printf("T10 buzzer: low-water alarm\n");
  resetState();
  checkBuzzer(false);
  gWater = 100;
  controlAlerts();
  checkBuzzer(true);
  gWater = 800;
  controlAlerts();
  checkBuzzer(false);
}

// T11: servo cối xay gió quét (hoạt động 6)
static void testWindmillSweep() {
  printf("T11 windmill: servo sweep\n");
  resetState();
  setWindmill(true);
  CHECK(windmillOn);
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
  CHECK(!windmillOn);
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  setWindmill(true);           // cối xay gió lấy lại
  CHECK(!feedRunning());
  CHECK(windmillOn);
  setWindmill(false);
}

// T13: đoán thời tiết (hoạt động 11)
static void testWeather() {
  printf("T13 weather: light + humidity guess\n");
  resetState();
  gLight = 800; gHum = 50; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gHum = 90;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 200; gHum = 90; CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gHum = 50;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 500;            CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
}

// T14: lưu/đọc settings vòng tròn
static void testSettingsRoundtrip() {
  printf("T14 settings: save/load roundtrip\n");
  resetState();
  soilThresholdLow = 111; soilThresholdHigh = 222; waterThresholdLow = 133;
  lightThreshold = 654; tempThresholdHigh = 35.5f; tempThresholdLow = 21.5f;
  pumpAuto = false; lightAuto = false; rgbAuto = true; fanAuto = false; breatheAuto = true;
  saveSettings();

  soilThresholdLow = 0; soilThresholdHigh = 0; waterThresholdLow = 0;
  lightThreshold = 0; tempThresholdHigh = 0; tempThresholdLow = 0;
  pumpAuto = true; lightAuto = true; rgbAuto = false; fanAuto = true; breatheAuto = false;
  loadSettings();

  CHECK_NEAR(soilThresholdLow, 111, 0.01);
  CHECK_NEAR(soilThresholdHigh, 222, 0.01);
  CHECK_NEAR(waterThresholdLow, 133, 0.01);
  CHECK_EQ(lightThreshold, 654);
  CHECK_NEAR(tempThresholdHigh, 35.5, 0.01);
  CHECK_NEAR(tempThresholdLow, 21.5, 0.01);
  CHECK(!pumpAuto); CHECK(!lightAuto); CHECK(rgbAuto); CHECK(!fanAuto); CHECK(breatheAuto);
}

// T15: chạy demo đầy đủ (hoạt động 2-12)
static void testDemoFullRun() {
  printf("T15 demo: full sequence\n");
  resetState();
  primeSensors(900);
  CHECK(gWater > (int)waterThresholdLow);

  startDemo();
  CHECK(demoRunning());
  bool sawRelay = false, inBreatheStep = false;
  uint32_t breatheMin = 9999, breatheMax = 0;
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (relayState) sawRelay = true;
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
  CHECK(!relayState);
  CHECK(!blueLedState);
  CHECK(!breatheLedState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);
  CHECK_EQ(mockPixel[0], 0);

  // Hết nước: bước bơm phải bị bỏ qua suốt lượt chạy
  resetState();
  primeSensors(100);
  CHECK(gWater < (int)waterThresholdLow);
  startDemo();
  sawRelay = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (relayState) sawRelay = true;
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
  gWater = 900;
  gSoil = 400; controlPump(); CHECK(!relayState);            // == low: không khởi động
  gSoil = 399; controlPump(); CHECK(relayState);             // < low: khởi động
  gSoil = 700; controlPump(); CHECK(relayState);             // == high: vẫn bật
  gSoil = 701; controlPump(); CHECK(!relayState);            // > high: dừng
  gWater = 200; gSoil = 399; controlPump(); CHECK(relayState);  // == waterLow: được phép
  gWater = 199; controlPump(); CHECK(!relayState);           // < waterLow: bị chặn

  // Quạt: nghiêm ngặt > / <
  resetState();
  gTemp = 30;    controlFan(); CHECK(!fanState);             // == high: không khởi động
  gTemp = 30.01f; controlFan(); CHECK(fanState);             // > high: khởi động
  gTemp = 25;    controlFan(); CHECK(fanState);              // == low: vẫn bật
  gTemp = 24.99f; controlFan(); CHECK(!fanState);            // < low: dừng

  // Đèn: ngưỡng +/- 50 với so sánh nghiêm ngặt
  resetState();
  gLight = 450; controlLight(); CHECK(!blueLedState);        // == t-50: không bật
  gLight = 449; controlLight(); CHECK(blueLedState);         // < t-50: bật
  gLight = 550; controlLight(); CHECK(blueLedState);         // == t+50: vẫn bật
  gLight = 551; controlLight(); CHECK(!blueLedState);        // > t+50: tắt

  // Thời tiết: cùng dải +/- 50 và độ ẩm > 80
  resetState();
  gHum = 50;
  gLight = 550; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gLight = 450; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 449; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 80;    CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);   // == 80 không tính là ẩm
  gHum = 81;    CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 50.5f; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);

  // Còi: nghiêm ngặt < waterLow
  resetState();
  gPir = false;
  gWater = 200; controlAlerts(); checkBuzzer(false);
  gWater = 199; controlAlerts(); checkBuzzer(true);
  gWater = 200; controlAlerts(); checkBuzzer(false);
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
}

// D3: biên thời gian chính xác + ghi servo tại các mốc biên
static void testTimingBoundaries() {
  printf("D3  timing: exact interval boundaries\n");

  // Debounce chạm: >= TOUCH_DEBOUNCE
  resetState();
  mockAdvance(10000);
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  gTouch = true; controlTouch();                  // delta 0: bị nuốt
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(TOUCH_DEBOUNCE - 1);                // 299: bị nuốt
  gTouch = true; controlTouch();
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(1);                                 // đúng 300: được phép
  gTouch = true; controlTouch();
  CHECK(blueLedState);

  // Cửa cho ăn: đóng đúng tại FEED_DURATION
  resetState();
  startFeed();
  mockAdvance(FEED_DURATION - 1); controlFeed(); CHECK(feedRunning());
  mockAdvance(1);                 controlFeed(); CHECK(!feedRunning());

  // Breathing: cập nhật đầu tiên đúng tại BREATHE_INTERVAL
  resetState();
  setBreatheLed(true);
  setLedPwm(0);
  lastBreatheUpdate = mockMillis();
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
  servoAngle = 0;   servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 0);
  servoAngle = 180; servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 180);
  servoAngle = 0;   servo.write(servoAngle);
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
  CHECK_EQ(mockServerRouteCount(), 15);
  const char *routes[] = {"/", "/api", "/toggle", "/auto", "/rgb", "/servo", "/breathe",
                          "/on", "/off", "/windmill", "/feed", "/demo", "/threshold", "/favicon.ico"};
  for (const char *r : routes) CHECK(mockServerHasRoute(r));
  CHECK(has(mockSerialOutput(), "SMART FARM DEBUG MENU"));
  CHECK(has(mockSerialOutput(), "System ready"));
}

// I2: JSON contract của /api
static void testHttpApi() {
  printf("I2  http: /api JSON\n");
  gTemp = 26.5f; gHum = 61.2f;
  gLight = 800; gSoil = 650; gWater = 900;
  gPir = true; gTouch = false;
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
  CHECK(blueLedState);
  CHECK(!lightAuto);
  CHECK_STR(mockServerLastBody(), "LED ON");
  CHECK_EQ(invoke("/off"), 200);
  CHECK(!blueLedState);
  CHECK_STR(mockServerLastBody(), "LED OFF");
}

// I4: cơ cấu servo qua HTTP (HD6/HD10)
static void testHttpMechanisms() {
  printf("I4  http: windmill / feed / servo arbitration\n");
  invoke("/windmill", "on", "true");
  CHECK(windmillOn);
  invoke("/windmill", "on", "false");
  CHECK(!windmillOn);

  invoke("/feed");
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);

  invoke("/servo", "angle", "45");           // điều khiển thủ công giải phóng cơ cấu
  CHECK_EQ(servoAngle, 45);
  CHECK_EQ(mockServoAngle, 45);
  CHECK(!feedRunning());
  invoke("/servo", "angle", "999");          // bị kẹp
  CHECK_EQ(servoAngle, 180);
  invoke("/servo", "angle", "-5");           // bị kẹp
  CHECK_EQ(servoAngle, 0);

  invoke("/demo");
  CHECK(demoRunning());
  runLoop(40000);                            // chạy toàn bộ demo qua loop()
  CHECK(!demoRunning());
}

// I5: toggle / auto / rgb / threshold
static void testHttpControlRoutes() {
  printf("I5  http: toggle/auto/rgb/threshold\n");
  invoke("/toggle", "id", "pump");
  CHECK(!pumpAuto);
  CHECK(relayState);
  invoke("/toggle", "id", "pump");
  CHECK(!relayState);
  CHECK_EQ(invoke("/toggle", "id", "bogus"), 400);

  invoke("/auto", "id", "rgb", "on", "false");
  CHECK(!rgbAuto);

  invoke("/rgb", "r", "300", "g", "-5", "b", "128");
  CHECK_EQ(rgbR, 255);
  CHECK_EQ(rgbG, 0);
  CHECK_EQ(rgbB, 128);
  CHECK(!rgbAuto);
  CHECK_EQ(mockPixel[1], (255 << 16) | (0 << 8) | 128);

  invoke("/auto", "id", "breathe", "on", "true");
  CHECK(breatheAuto);
  CHECK(breatheLedState);
  runLoop(2000);
  uint32_t peak = 0;
  for (int i = 0; i < 60; i++) {
    mockAdvance(BREATHE_INTERVAL);
    loop();
    if (currentLedPwm() > peak) peak = currentLedPwm();
  }
  CHECK(peak > 0);                           // breathing được animate qua loop()
  invoke("/auto", "id", "breathe", "on", "false");
  CHECK(!breatheAuto);
  CHECK(!breatheLedState);
  CHECK_EQ(invoke("/auto", "id", "bogus", "on", "true"), 400);

  invoke("/threshold", "soilLow", "2000");
  CHECK_NEAR(soilThresholdLow, 2000, 0.01);
  CHECK(soilThresholdHigh > soilThresholdLow);   // tự sửa thứ tự
  CHECK_NEAR(mockPrefGet("soilLow", 0), 2000, 0.01);  // đã lưu bền
  invoke("/threshold", "soilLow", "9000");
  CHECK_NEAR(soilThresholdLow, 4095, 0.01);      // bị kẹp
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);
  CHECK_EQ(invoke("/threshold"), 400);           // không có tham số
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "tempLow", "50");         // dải nhiệt độ đảo được tự sửa
  CHECK_NEAR(tempThresholdLow, 50, 0.01);
  CHECK(tempThresholdHigh > tempThresholdLow);
  CHECK_NEAR(mockPrefGet("tempHigh", 0), tempThresholdHigh, 0.01);
  tempThresholdHigh = 30; tempThresholdLow = 25;
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

// I7: render OLED
static void testOled() {
  printf("I7  oled: status frame\n");
  mockWifiStatus = WL_CONNECTED;
  gTemp = 26.5f; gHum = 61; gLight = 800; gSoil = 650; gWater = 900;
  relayState = false;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
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
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "WiFi: ---"));
  mockWifiStatus = WL_CONNECTED;

  windmillOn = true;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "Coi xay: ON"));
  windmillOn = false;
  startFeed();
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
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
    mockSerialPush(&c.cmd);
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

  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "HD2: Thap sang"));      // tên bước trên OLED

  runLoop(40000);
  CHECK(!demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "San sang"));
}

// I10: xử lý mất/kết nối lại WiFi
static void testWifiRecovery() {
  printf("I10 wifi: loss + reconnect logging\n");
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi lost"));
  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
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

  if (fabs(numOf(pairs, "temp") - gTemp) > 0.06) { err = "temp mismatch"; return false; }
  if (fabs(numOf(pairs, "hum") - gHum) > 0.06) { err = "hum mismatch"; return false; }
  if ((long)numOf(pairs, "light") != gLight) { err = "light mismatch"; return false; }
  if ((long)numOf(pairs, "soil") != gSoil) { err = "soil mismatch"; return false; }
  if ((long)numOf(pairs, "water") != gWater) { err = "water mismatch"; return false; }
  if ((int)numOf(pairs, "weather") != (int)weatherStatus()) { err = "weather mismatch"; return false; }

  if (boolOf(pairs, "pir") != gPir) { err = "pir mismatch"; return false; }
  if (boolOf(pairs, "touch") != gTouch) { err = "touch mismatch"; return false; }
  if (boolOf(pairs, "relay") != relayState) { err = "relay mismatch"; return false; }
  if (boolOf(pairs, "fan") != fanState) { err = "fan mismatch"; return false; }
  if (boolOf(pairs, "led") != blueLedState) { err = "led mismatch"; return false; }
  if (boolOf(pairs, "breathe") != breatheLedState) { err = "breathe mismatch"; return false; }
  if (boolOf(pairs, "windmill") != windmillOn) { err = "windmill mismatch"; return false; }
  if (boolOf(pairs, "feed") != feedRunning()) { err = "feed mismatch"; return false; }
  if (boolOf(pairs, "demo") != demoRunning()) { err = "demo mismatch"; return false; }
  if (boolOf(pairs, "pumpAuto") != pumpAuto) { err = "pumpAuto mismatch"; return false; }
  if (boolOf(pairs, "lightAuto") != lightAuto) { err = "lightAuto mismatch"; return false; }
  if (boolOf(pairs, "rgbAuto") != rgbAuto) { err = "rgbAuto mismatch"; return false; }
  if (boolOf(pairs, "fanAuto") != fanAuto) { err = "fanAuto mismatch"; return false; }
  if (boolOf(pairs, "breatheAuto") != breatheAuto) { err = "breatheAuto mismatch"; return false; }

  const JsonPair *ds = findPair(pairs, "demoStep");
  const char *expectedStep = demoRunning() ? demoStepName() : "";
  if (ds->raw != expectedStep) { err = "demoStep mismatch"; return false; }

  return true;
}

// D5: JSON contract nghiêm ngặt của /api
static void testApiJsonContract() {
  printf("D5  api: strict JSON contract (21 keys)\n");
  std::string err;

  // Trạng thái đã biết
  gTemp = 26.5f; gHum = 61.2f;
  gLight = 800; gSoil = 650; gWater = 900;
  gPir = true; gTouch = false;
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
    gTemp = (float)(rng() % 500) / 10.0f;
    gHum = (float)(rng() % 101);
    gLight = (int)(rng() % 4096);
    gSoil = (int)(rng() % 4096);
    gWater = (int)(rng() % 4096);
    gPir = (rng() % 2) != 0;
    gTouch = (rng() % 2) != 0;
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
  CHECK(!lightAuto);                       // chỉ "true" mới bật auto

  invoke("/rgb", "r", "0", "g", "255", "b", "255");
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbG, 255); CHECK_EQ(rgbB, 255);
  CHECK_STR(mockServerLastBody(), "ok");

  invoke("/breathe");
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK(breatheLedState);
  CHECK(!breatheAuto);
  invoke("/breathe");
  CHECK(!breatheLedState);

  invoke("/windmill");                     // không tham số -> tắt
  CHECK_STR(mockServerLastBody(), "windmill off");
  invoke("/windmill", "on", "true");
  CHECK_STR(mockServerLastBody(), "windmill on");
  CHECK(windmillOn);
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
  CHECK_NEAR(soilThresholdLow, 4095, 0.01);
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);        // mức sửa tốt nhất tại biên
  // Ngưỡng bằng nhau không được làm bơm bật/tắt liên tục (guard điều khiển):
  gWater = 900; gSoil = 100; relayState = false; pumpAuto = true;
  controlPump();
  CHECK(!relayState);
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "soilLow", "-5");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);
  CHECK_NEAR(soilThresholdHigh, 700, 0.01);         // không đổi (vẫn > low)
  invoke("/threshold", "soilLow", "abc");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);            // toFloat("abc") trả về 0
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "waterLow", "99999");
  CHECK_NEAR(waterThresholdLow, 4095, 0.01);
  waterThresholdLow = 200;

  invoke("/threshold", "tempLow", "60");
  CHECK_NEAR(tempThresholdLow, 60, 0.01);
  CHECK_NEAR(tempThresholdHigh, 61, 0.01);          // giữ nghiêm ngặt trên low
  invoke("/threshold", "tempLow", "25");
  CHECK_NEAR(tempThresholdLow, 25, 0.01);
  invoke("/threshold", "tempHigh", "0");
  CHECK_NEAR(tempThresholdHigh, 26, 0.01);          // sửa thành low + 1
  tempThresholdHigh = 30; tempThresholdLow = 25;
  invoke("/threshold", "light", "5000");
  CHECK_EQ(lightThreshold, 4095);
  lightThreshold = 500;

  invoke("/rgb");                                    // không tham số -> toàn số 0
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbG, 0); CHECK_EQ(rgbB, 0);
  CHECK(!rgbAuto);
  invoke("/servo", "angle", "0");
  CHECK_EQ(servoAngle, 0);
  invoke("/servo", "angle", "180");
  CHECK_EQ(servoAngle, 180);
  invoke("/servo", "angle", "abc");
  CHECK_EQ(servoAngle, 0);
  servoAngle = 0;
}

// D7: bố cục từng dòng OLED + giới hạn độ dài dòng
static void testOledLines() {
  printf("D7  oled: per-row layout + 21-char line limit\n");
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "255.255.255.255";
  gTemp = 26.5f; gHum = 61; gLight = 800; gSoil = 650; gWater = 900;
  relayState = false; windmillOn = false;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK_STR(mockOledLine(0), "Smart Farm");
  CHECK_STR(mockOledLine(1), "TT: Nang");
  CHECK_STR(mockOledLine(2), "T:26.5C H:61%");
  CHECK_STR(mockOledLine(3), "Light: 800");
  CHECK_STR(mockOledLine(4), "Soil: 650");
  CHECK_STR(mockOledLine(5), "H2O:900 P:OFF");
  CHECK_STR(mockOledLine(6), "IP:255.255.255.255");
  CHECK_STR(mockOledLine(7), "San sang");

  // Giá trị xấu nhất: mỗi dòng vẫn phải vừa 21 ký tự (128 px / 6 px)
  gTemp = 99.9f; gHum = 100; gLight = 4095; gSoil = 4095; gWater = 4095;
  relayState = true;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  for (int r = 0; r < mockOledLineCount(); r++) {
    CHECK(strlen(mockOledLine(r)) <= 21);
    CHECK(strlen(mockOledLine(r)) > 0);
  }
  CHECK_STR(mockOledLine(5), "H2O:4095 P:ON");
  relayState = false;
  mockWifiIp = "192.168.1.50";
}

// D8: các ca biên serial + hành vi debug-mode
static void testSerialEdges() {
  printf("D8  serial: edge cases + debug mode\n");

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
  gWater = 900; gSoil = 100; pumpAuto = true;
  controlPump();
  CHECK(relayState);
  mockSerialClear();
  mockSerialPush("3");
  loop();
  CHECK(has(mockSerialOutput(), "--- CONTROL ---"));
  gWater = 50;
  mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
  lastSensorRead = mockMillis();
  loop();
  CHECK(relayState);                          // chế độ debug: automation tạm dừng
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
  CHECK(!relayState);

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
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK_STR(mockOledLine(7), names.back().c_str());
  CHECK(strlen(names.back().c_str()) <= 21);

  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (!demoRunning()) break;
    const char *n = demoStepName();
    if (names.back() != n) {
      names.push_back(n);
      updateOLED(gTemp, gHum, gLight, gSoil, gWater);
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
  CHECK(!relayState);
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
      gWater = 50;                                     // bồn cạn giữa bước
      mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
      lastSensorRead = mockMillis();
    }
    simLoop();
    if (relayState) sawPumpOn = true;
    if (relayState && gWater < (int)waterThresholdLow) violated = true;
  }
  CHECK(dropped);
  CHECK(sawPumpOn);                                    // bơm chạy khi nước còn ổn
  CHECK(!violated);                                    // sau đó không bao giờ chạy khô
  CHECK(!demoRunning());
  CHECK(!relayState);
}

// D10: bộ đếm throttle kết nối lại WiFi
static void testWifiThrottle() {
  printf("D10 wifi: reconnect throttle + counters\n");
  mockWifiStatus = WL_CONNECTED;
  const int c0 = mockWifiBeginCalls;
  loop();                                              // reset cờ đã cảnh báo
  CHECK_EQ(mockWifiBeginCalls, c0);                    // đã kết nối: không thử kết nối lại

  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis();
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
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // không thử thêm

  // Một lần mất mạng mới lại ghi log (cờ cảnh báo đã được reset bởi lần kết nối lại)
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);
  mockWifiStatus = WL_CONNECTED;
  loop();
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
  CHECK_EQ(mockServerRouteCount(), 15);
}

// D12: settings sống sót qua chu kỳ nguồn mô phỏng
static void testRebootPersistence() {
  printf("D12 settings: power-cycle persistence\n");
  invoke("/threshold", "soilLow", "1234");
  CHECK_NEAR(soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(soilThresholdHigh, 1284, 0.01);
  invoke("/auto", "id", "pump", "on", "false");
  invoke("/auto", "id", "breathe", "on", "true");
  invoke("/toggle", "id", "fan");
  CHECK(!pumpAuto);
  CHECK(breatheAuto);
  CHECK(!fanAuto);
  CHECK(fanState);

  // chu kỳ nguồn: flash giữ prefs, trạng thái chỉ trong RAM bị mất
  mockServerReset();
  fanState = relayState = windmillOn = blueLedState = breatheLedState = false;
  rgbR = rgbG = rgbB = 0;
  setup();

  CHECK_EQ(mockServerRouteCount(), 15);
  CHECK_NEAR(soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(soilThresholdHigh, 1284, 0.01);
  CHECK(!pumpAuto);
  CHECK(!fanAuto);
  CHECK(breatheAuto);
  CHECK(breatheLedState);                              // setup bật lại breathing
  CHECK(!fanState);                                    // trạng thái runtime được reset
  CHECK_EQ(rgbR, 0);                                   // RGB thủ công không được lưu bền
}

// D13: fuzz/soak có seed với các safety invariant
static void checkFuzzInvariants(bool demoAtLoopStart) {
  // 1. Bơm không bao giờ chạy khi hết nước (safety line + controlPump).
  CHECK(!(gWater < (int)waterThresholdLow && relayState));
  // 2. Mức chân luôn khớp với trạng thái logic.
  CHECK((mockPinLevel[RELAY_PIN] != 0) == (relayState != (bool)RELAY_ACTIVE_LOW));
  CHECK((mockPinLevel[FAN_PIN] != 0) == fanState);
  // 3. Các cơ cấu servo dùng chung không bao giờ chạy cùng lúc.
  CHECK(!(windmillOn && feedRunning()));
  // 4. Các chế độ LED xanh loại trừ lẫn nhau.
  CHECK(!(blueLedState && breatheLedState));
  // 5. Ghi servo và PWM luôn nằm trong dải hợp lệ.
  CHECK(mockServoMinSeen >= 0 && mockServoMaxSeen <= 180);
  CHECK_EQ(mockInvalidPixelWrites, 0);
  CHECK_EQ(mockInvalidPwmWrites, 0);
  // 6. Không còn PWM cũ khi cả hai chế độ LED đều tắt.
  if (!blueLedState && !breatheLedState) CHECK_EQ(currentLedPwm(), 0);
  // 7. Khi còi đang báo động (không phải demo), còi phải thực sự bật.
  // Bỏ qua loop mà demo đang chạy vừa kết thúc: automation (và
  // còi báo) chỉ hoạt động lại ở loop kế tiếp.
  if (!demoRunning() && !demoAtLoopStart && gWater < (int)waterThresholdLow) {
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
        gWater = (int)(rng() % 4096);
        mockAdc[WATER_PIN] = INVERT_WATER ? 4095 - gWater : gWater;
        gSoil = (int)(rng() % 4096);
        mockAdc[SOIL_PIN] = INVERT_SOIL ? 4095 - gSoil : gSoil;
        gLight = (int)(rng() % 4096);
        mockAdc[LDR_PIN] = INVERT_LIGHT ? 4095 - gLight : gLight;
        gTemp = (float)(rng() % 500) / 10.0f;
        mockDhtTemp = gTemp;
        gHum = (float)(rng() % 101);
        mockDhtHum = gHum;
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
        CHECK(code == 200 || code == 204 || code == 400);
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
          case 0: setRelay(!relayState); break;
          case 1: setFan(!fanState); break;
          case 2: setBlueLed(!blueLedState); break;
          case 3: setBreatheLed(!breatheLedState); break;
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

  fuzzRun(1,    30000);
  fuzzRun(42,   30000);
  fuzzRun(2026, 30000);

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
  lastSensorRead = mockMillis();
  runLoop(5100);                             // vượt mốc wrap tại +256 ms
  CHECK_NEAR(gTemp, 41.0, 0.01);
  CHECK_NEAR(gHum, 33.0, 0.01);

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
  lastBreatheUpdate = mockMillis();
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
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  bool before = blueLedState;
  mockAdvance(TOUCH_DEBOUNCE);               // xóa cửa sổ debounce trước
  gTouch = true; controlTouch();
  CHECK(blueLedState != before);
  gTouch = false; controlTouch();
  setBlueLed(false);

  // E: demo đầy đủ phải kết thúc sạch khi mốc wrap xảy ra giữa lượt chạy
  mockSetMillis(0xFFFFFF00UL);
  primeSensors(900);
  startDemo();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
  }
  CHECK(!demoRunning());
  CHECK(!relayState);
  CHECK_EQ(currentLedPwm(), 0);

  // F: timer thử lại WiFi trải qua mốc wrap
  mockSetMillis(0xFFFFFF00UL);
  mockWifiStatus = WL_CONNECTED;
  loop();                                            // xóa cờ đã cảnh báo
  int b0 = mockWifiBeginCalls;
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = 0xFFFFFF00UL;
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
  gPir = false; controlAlerts();          // xóa mọi trạng thái cảnh báo đang chờ
  setBreatheAuto(true);
  setBreatheLed(true);
  CHECK(breatheLedState);
  gPir = true;
  controlAlerts();
  CHECK(blueLedState);
  CHECK(!breatheLedState);                // cảnh báo đã chiếm LED
  gPir = false;
  controlAlerts();
  CHECK(!blueLedState);
  CHECK(breatheLedState);                 // breathing tự chọn được bật lại
  CHECK(breatheAuto);
  setBreatheAuto(false);
  setBreatheLed(false);

  // LED bật thủ công không bị ảnh hưởng khi có người đi qua (PIR không sở hữu nó)
  resetState();
  gPir = false; controlAlerts();
  setBlueLed(true);
  gPir = true;  controlAlerts();
  CHECK(blueLedState);
  gPir = false; controlAlerts();
  CHECK(blueLedState);
  setBlueLed(false);

  // Không có breathing auto, cảnh báo chỉ tắt LED trở lại
  resetState();
  gPir = false; controlAlerts();
  gPir = true;  controlAlerts();
  CHECK(blueLedState);
  gPir = false; controlAlerts();
  CHECK(!blueLedState);
  CHECK(!breatheLedState);

  // Phòng thủ: giá trị ramp cũ được xóa khi breathing tắt
  resetState();
  breatheBrightness = 100;
  setLedPwm(222);
  controlBreathe();
  CHECK_EQ(breatheBrightness, 0);
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
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); simLoop(); }
  CHECK(!demoRunning());
  CHECK(breatheAuto);
  CHECK(breatheLedState);                 // breathing bật lại sau DEMO DONE
  CHECK(!blueLedState);
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
  bool before = relayState;
  CHECK_EQ(invoke("/toggle", "id", "pump"), 200);
  CHECK(relayState != before);

  // Origin lạ (trang web khác gọi fetch/XHR) → 403, trạng thái không đổi
  before = relayState;
  mockServerSetHeader("Origin", "http://evil.example");
  CHECK_EQ(invoke("/toggle", "id", "pump"), 403);
  CHECK(relayState == before);
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
  CHECK(windmillOn);

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
  bool before = relayState;
  mockServerSetArg("id", "pump");
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 401);
  CHECK_STR(mockServerLastBody(), "unauthorized");
  CHECK(relayState == before);

  // Token sai qua query → 401
  mockServerSetArg("id", "pump");
  mockServerSetArg("token", "sai-token");
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 401);
  CHECK(relayState == before);

  // Token đúng qua query → 200
  mockServerSetArg("id", "pump");
  mockServerSetArg("token", WEB_TOKEN);
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 200);
  CHECK(relayState != before);

  // Token đúng qua header X-Auth-Token → 200
  before = relayState;
  mockServerSetArg("id", "pump");
  mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
  CHECK(mockServerInvoke("/toggle"));
  CHECK_EQ(mockServerLastCode(), 200);
  CHECK(relayState != before);

  resetState();
}

// I11: boot suy giảm - thiếu OLED và timeout kết nối WiFi
static void testBootDegraded() {
  printf("I11 boot: missing OLED + WiFi timeout\n");
  resetState();
  mockServerReset();
  mockDisplayBeginOk = false;
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  setup();

  CHECK_EQ(mockServerRouteCount(), 15);              // route vẫn được đăng ký
  CHECK(has(mockSerialOutput(), "MA LOI: 101"));     // không tìm thấy OLED
  CHECK(has(mockSerialOutput(), "MA LOI: 102"));     // WiFi timeout
  CHECK(mockWdtResetCalls >= 100);                   // watchdog được feed trong lúc chờ
  CHECK(mockWifiBeginCalls >= 1);
  CHECK(!demoRunning());

  mockDisplayBeginOk = true;
  mockWifiStatus = WL_CONNECTED;
  resetState();
}

// ==============================================
//  Hàm chính
// ==============================================

int main() {
  printf("=== Smart Farm deep test suite (core %d, invert L/S/W %d/%d/%d, relayLO %d, buzzPassive %d) ===\n",
         ESP_ARDUINO_VERSION_MAJOR, INVERT_LIGHT, INVERT_SOIL, INVERT_WATER,
         RELAY_ACTIVE_LOW, BUZZER_PASSIVE);

  testSensorsAverage();
  testDhtFailover();
  testPumpHysteresis();
  testPumpLowWaterSafety();
  testFanHysteresis();
  testLightPirAlerts();
  testBreatheAndLedModes();
  testTouchToggle();
  testRgbPlant();
  testBuzzerAlarm();
  testWindmillSweep();
  testFeedGate();
  testWeather();
  testSettingsRoundtrip();
  testDemoFullRun();
  testAlertLedOwnership();
  testDemoBreathingRestore();

  testBoundaries();
  testIdempotency();
  testTimingBoundaries();

  printf("--- integration ---\n");
  testInoBoot();
  testHttpApi();
  testHttpLedOnOff();
  testHttpMechanisms();
  testHttpControlRoutes();
  testHttpRootFavicon();
  testOled();
  testSerialMenu();
  testSerialDemo();
  testWifiRecovery();

  printf("--- deep ---\n");
  testApiJsonContract();
  testResponseExactness();
  testOledLines();
  testSerialEdges();
  testDemoDiagnostics();
  testWifiThrottle();
  testBootOrder();
  testRebootPersistence();
  testFuzzSoak();
  testClockWrap();
  testWebSecurity();
  testWebToken();
  testBootDegraded();

  printf("=== %d checks, %d failures ===\n", gChecks, gFails);
  return gFails ? 1 : 0;
}
