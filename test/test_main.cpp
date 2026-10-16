// Deep host-side test suite for the Smart Farm QE002 firmware.
//
// The suite compiles the REAL firmware (SmartFarm.ino + src/*.cpp) against the
// controllable hardware mocks in test/mocks/ and exercises:
//   - unit tests for every control-logic module
//   - integration tests through the real setup()/loop() and HTTP handlers
//   - deep tests: strict JSON contract, boundary values, millis() wrap,
//     demo mid-step safety, WiFi throttle counters, debug-mode behaviour,
//     settings persistence across a simulated power cycle, and a seeded
//     fuzz/soak pass that checks safety invariants after every loop().
//
// Build & run:  ./test/run_tests.sh   (see test/README.md)
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

// The real firmware entry points (SmartFarm.ino)
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
//  Helpers
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

static void resetState() {
  mockReset();
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

// Mirrors the control section of SmartFarm.ino loop() (without HTTP/OLED)
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
//  Unit tests
// ==============================================

// T1: sensor averaging + inversion direction
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

// T2: DHT NaN failover + health flag
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
}

// T3: pump soil hysteresis (activity 7/10)
static void testPumpHysteresis() {
  printf("T3  pump: soil hysteresis\n");
  resetState();
  gWater = 900;
  gSoil = 300;
  controlPump();
  CHECK(relayState);
  checkRelayPin(true);
  gSoil = 500;                 // between thresholds: no change
  controlPump();
  CHECK(relayState);
  gSoil = 800;
  controlPump();
  CHECK(!relayState);
  checkRelayPin(false);

  // Inverted thresholds must not make the relay chatter
  resetState();
  soilThresholdLow = 700; soilThresholdHigh = 400;
  gSoil = 500; gWater = 900;
  controlPump();
  CHECK(!relayState);
  controlPump();
  CHECK(!relayState);
}

// T4: pump low-water safety even in manual mode (activity 8)
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
  gSoil = 100;                 // very dry but no water
  gWater = 100;
  controlPump();
  CHECK(!relayState);
}

// T5: fan temperature hysteresis
static void testFanHysteresis() {
  printf("T5  fan: temperature hysteresis\n");
  resetState();
  gTemp = 31; controlFan(); CHECK(fanState);
  gTemp = 27; controlFan(); CHECK(fanState);
  gTemp = 24; controlFan(); CHECK(!fanState);

  // Inverted thresholds must not make the relay chatter
  resetState();
  tempThresholdHigh = 25; tempThresholdLow = 30;
  gTemp = 27;
  controlFan();
  CHECK(!fanState);
  controlFan();
  CHECK(!fanState);
}

// T6: light auto + PIR takeover + alerts
static void testLightPirAlerts() {
  printf("T6  light auto / PIR / alerts\n");
  resetState();
  gLight = 400;                // dark
  controlLight();
  CHECK(blueLedState);
  gLight = 600;                // bright
  controlLight();
  CHECK(!blueLedState);

  gPir = true;                 // PIR owns the LED: auto light must yield
  gLight = 400;
  controlLight();
  CHECK(!blueLedState);

  controlAlerts();             // PIR turns the light on
  CHECK(blueLedState);
  gPir = false;
  controlAlerts();             // and off when motion stops
  CHECK(!blueLedState);
}

// T7: blue LED steady vs breathing (activity 3) + PWM dispatch
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

  setBlueLed(true);            // steady command wins over breathing
  CHECK(!breatheLedState);
  CHECK(blueLedState);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);

  setLedPwm(123);
  CHECK_EQ(currentLedPwm(), 123);
}

// T8: touch toggle with debounce (activity 4)
static void testTouchToggle() {
  printf("T8  touch: edge toggle + debounce\n");
  resetState();
  mockAdvance(5000);
  gTouch = true;
  controlTouch();
  CHECK(blueLedState);
  controlTouch();              // held: no repeat
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();
  gTouch = true;
  mockAdvance(TOUCH_DEBOUNCE - 100);
  controlTouch();              // inside debounce: press swallowed
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();              // release
  mockAdvance(200);            // now past the debounce window
  gTouch = true;
  controlTouch();              // toggles off
  CHECK(!blueLedState);
  CHECK(!lightAuto);           // manual action disabled auto
}

// T9: RGB plant growth light (activity 9)
static void testRgbPlant() {
  printf("T9  RGB: plant pattern (activity 9)\n");
  resetState();
  setRgb(10, 20, 30);
  CHECK_EQ(mockPixel[3], (10 << 16) | (20 << 8) | 30);

  gLight = 400;                // dark -> plant pattern
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
  CHECK_EQ(mockPixel[1], 0x0000FF);
  CHECK_EQ(mockPixel[2], 0xFF0000);
  CHECK_EQ(mockPixel[3], 0x0000FF);
  CHECK_EQ(rgbR, 255); CHECK_EQ(rgbG, 0); CHECK_EQ(rgbB, 255);

  gLight = 600;                // bright -> off
  controlRgb();
  for (int i = 0; i < RGB_COUNT; i++) CHECK_EQ(mockPixel[i], 0);
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbB, 0);

  setRgb(10, 20, 30);          // manual color, then dark again
  gLight = 400;
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
}

// T10: low-water buzzer alarm (activity 8)
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

// T11: windmill servo sweep (activity 6)
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

// T12: feed gate timing + shared servo arbitration (activity 10)
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
  startFeed();                 // feed takes the shared servo
  CHECK(feedRunning());
  CHECK(!windmillOn);
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  setWindmill(true);           // windmill takes it back
  CHECK(!feedRunning());
  CHECK(windmillOn);
  setWindmill(false);
}

// T13: weather guess (activity 11)
static void testWeather() {
  printf("T13 weather: light + humidity guess\n");
  resetState();
  gLight = 800; gHum = 50; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gHum = 90;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 200; gHum = 90; CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gHum = 50;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 500;            CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
}

// T14: settings save/load roundtrip
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

// T15: full demo run (activities 2-12)
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
  CHECK(sawRelay);                       // HD7 pumped with water available
  CHECK(inBreatheStep);                  // HD3 step ran
  CHECK(breatheMax > 0);                 // and the LED actually breathed
  CHECK(breatheMin < breatheMax);
  CHECK(!relayState);
  CHECK(!blueLedState);
  CHECK(!breatheLedState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);
  CHECK_EQ(mockPixel[0], 0);

  // Low water: pump step must be skipped for the whole run
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

// D1: exact threshold boundaries
static void testBoundaries() {
  printf("D1  boundary: exact threshold comparisons\n");

  // Pump: strict < / > comparisons
  resetState();
  gWater = 900;
  gSoil = 400; controlPump(); CHECK(!relayState);            // == low: no start
  gSoil = 399; controlPump(); CHECK(relayState);             // < low: start
  gSoil = 700; controlPump(); CHECK(relayState);             // == high: stays on
  gSoil = 701; controlPump(); CHECK(!relayState);            // > high: stop
  gWater = 200; gSoil = 399; controlPump(); CHECK(relayState);  // == waterLow: allowed
  gWater = 199; controlPump(); CHECK(!relayState);           // < waterLow: blocked

  // Fan: strict > / <
  resetState();
  gTemp = 30;    controlFan(); CHECK(!fanState);             // == high: no start
  gTemp = 30.01f; controlFan(); CHECK(fanState);             // > high: start
  gTemp = 25;    controlFan(); CHECK(fanState);              // == low: stays on
  gTemp = 24.99f; controlFan(); CHECK(!fanState);            // < low: stop

  // Light: threshold +/- 50 with strict comparisons
  resetState();
  gLight = 450; controlLight(); CHECK(!blueLedState);        // == t-50: no on
  gLight = 449; controlLight(); CHECK(blueLedState);         // < t-50: on
  gLight = 550; controlLight(); CHECK(blueLedState);         // == t+50: stays on
  gLight = 551; controlLight(); CHECK(!blueLedState);        // > t+50: off

  // Weather: same +/- 50 band and hum > 80
  resetState();
  gHum = 50;
  gLight = 550; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gLight = 450; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 449; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 80;    CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);   // == 80 is not humid
  gHum = 81;    CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 50.5f; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);

  // Buzzer: strict < waterLow
  resetState();
  gPir = false;
  gWater = 200; controlAlerts(); checkBuzzer(false);
  gWater = 199; controlAlerts(); checkBuzzer(true);
  gWater = 200; controlAlerts(); checkBuzzer(false);
}

// D2: actuator idempotency + pin wiring consistency
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
  setRelay(true);                                // idempotent: no re-write
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
  setBlueLed(true);                              // idempotent: no re-write
  CHECK_EQ(mockLedcWriteCount, p0);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);
  setBreatheLed(false);
  int p1 = mockLedcWriteCount;
  setBreatheLed(false);                          // already off: no-op
  CHECK_EQ(mockLedcWriteCount, p1);
}

// D3: exact timing boundaries + servo writes at the edges
static void testTimingBoundaries() {
  printf("D3  timing: exact interval boundaries\n");

  // Touch debounce: >= TOUCH_DEBOUNCE
  resetState();
  mockAdvance(10000);
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  gTouch = true; controlTouch();                  // delta 0: swallowed
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(TOUCH_DEBOUNCE - 1);                // 299: swallowed
  gTouch = true; controlTouch();
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(1);                                 // 300 exactly: allowed
  gTouch = true; controlTouch();
  CHECK(blueLedState);

  // Feed gate: closes at exactly FEED_DURATION
  resetState();
  startFeed();
  mockAdvance(FEED_DURATION - 1); controlFeed(); CHECK(feedRunning());
  mockAdvance(1);                 controlFeed(); CHECK(!feedRunning());

  // Breathe: first update at exactly BREATHE_INTERVAL
  resetState();
  setBreatheLed(true);
  setLedPwm(0);
  lastBreatheUpdate = mockMillis();
  mockAdvance(BREATHE_INTERVAL - 1); controlBreathe(); CHECK_EQ(currentLedPwm(), 0);
  mockAdvance(1);                    controlBreathe(); CHECK_EQ(currentLedPwm(), 5);
  setBreatheLed(false);

  // Windmill: sweep step fires at exactly WINDMILL_INTERVAL
  resetState();
  setWindmill(true);
  mockAdvance(1000);
  controlWindmill();                              // sync the interval timer
  int c0 = mockServoWriteCount;
  mockAdvance(WINDMILL_INTERVAL - 1); controlWindmill(); CHECK_EQ(mockServoWriteCount, c0);
  mockAdvance(1);                     controlWindmill(); CHECK_EQ(mockServoWriteCount, c0 + 1);
  setWindmill(false);

  // Servo writes at the exact edges
  resetState();
  servoAngle = 0;   servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 0);
  servoAngle = 180; servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 180);
  servoAngle = 0;   servo.write(servoAngle);
}

// ==============================================
//  Integration tests (real setup/loop, HTTP, OLED, serial)
// ==============================================

// I1: firmware boot
static void testInoBoot() {
  printf("I1  boot: setup() registers routes\n");
  mockSerialClear();
  mockServerReset();
  setup();
  CHECK_EQ(mockServerRouteCount(), 14);
  const char *routes[] = {"/", "/api", "/toggle", "/auto", "/rgb", "/servo", "/breathe",
                          "/on", "/off", "/windmill", "/feed", "/demo", "/threshold", "/favicon.ico"};
  for (const char *r : routes) CHECK(mockServerHasRoute(r));
  CHECK(has(mockSerialOutput(), "SMART FARM DEBUG MENU"));
  CHECK(has(mockSerialOutput(), "System ready"));
}

// I2: /api JSON contract
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
  CHECK(has(b, "\"weather\":1"));            // bright + dry air -> sunny
  CHECK(has(b, "\"pir\":true"));
  CHECK(has(b, "\"touch\":false"));
  CHECK(has(b, "\"relay\":false"));
  CHECK(has(b, "\"windmill\":false"));
  CHECK(has(b, "\"feed\":false"));
  CHECK(has(b, "\"demo\":false"));
  CHECK(has(b, "\"pumpAuto\":"));
  CHECK(has(b, "\"breatheAuto\":"));
}

// I3: /on /off (activity 12)
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

// I4: servo mechanisms over HTTP (HD6/HD10)
static void testHttpMechanisms() {
  printf("I4  http: windmill / feed / servo arbitration\n");
  invoke("/windmill", "on", "true");
  CHECK(windmillOn);
  invoke("/windmill", "on", "false");
  CHECK(!windmillOn);

  invoke("/feed");
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);

  invoke("/servo", "angle", "45");           // manual control releases mechanisms
  CHECK_EQ(servoAngle, 45);
  CHECK_EQ(mockServoAngle, 45);
  CHECK(!feedRunning());
  invoke("/servo", "angle", "999");          // clamped
  CHECK_EQ(servoAngle, 180);
  invoke("/servo", "angle", "-5");           // clamped
  CHECK_EQ(servoAngle, 0);

  invoke("/demo");
  CHECK(demoRunning());
  runLoop(40000);                            // runs the whole demo through loop()
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
  CHECK(peak > 0);                           // breathing animated through loop()
  invoke("/auto", "id", "breathe", "on", "false");
  CHECK(!breatheAuto);
  CHECK(!breatheLedState);
  CHECK_EQ(invoke("/auto", "id", "bogus", "on", "true"), 400);

  invoke("/threshold", "soilLow", "2000");
  CHECK_NEAR(soilThresholdLow, 2000, 0.01);
  CHECK(soilThresholdHigh > soilThresholdLow);   // auto-fixed ordering
  CHECK_NEAR(mockPrefGet("soilLow", 0), 2000, 0.01);  // persisted
  invoke("/threshold", "soilLow", "9000");
  CHECK_NEAR(soilThresholdLow, 4095, 0.01);      // clamped
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);
  CHECK_EQ(invoke("/threshold"), 400);           // no params
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "tempLow", "50");         // inverted temp range auto-fixed
  CHECK_NEAR(tempThresholdLow, 50, 0.01);
  CHECK(tempThresholdHigh > tempThresholdLow);
  CHECK_NEAR(mockPrefGet("tempHigh", 0), tempThresholdHigh, 0.01);
  tempThresholdHigh = 30; tempThresholdLow = 25;
}

// I6: root page + favicon
static void testHttpRootFavicon() {
  printf("I6  http: root page + favicon\n");
  CHECK_EQ(invoke("/"), 200);
  CHECK_STR(mockServerLastType(), "text/html");
  CHECK(has(mockServerLastBody(), "Smart Farm"));
  CHECK(has(mockServerLastBody(), "/windmill"));
  CHECK(has(mockServerLastBody(), "/demo"));
  CHECK_EQ(invoke("/favicon.ico"), 204);
}

// I7: OLED rendering
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

// I8: serial debug menu
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
    CHECK(has(mockSerialOutput(), "Chon: "));       // menu reprinted
    mockSerialPush("0");
    loop();
    CHECK(has(mockSerialOutput(), "Running normally"));
  }

  mockSerialClear();
  mockSerialPush("x");                               // invalid
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));
  CHECK(has(mockSerialOutput(), "Chon: "));
  mockSerialClear();
}

// I9: demo via serial key '9'
static void testSerialDemo() {
  printf("I9  serial: '9' demo + OLED step name\n");
  mockSerialClear();
  mockSerialPush("9");
  loop();
  CHECK(demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO START"));

  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "HD2: Thap sang"));      // step name on OLED

  runLoop(40000);
  CHECK(!demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "San sang"));
}

// I10: WiFi loss/reconnect handling
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
//  Deep integration tests
// ==============================================

// Minimal flat-JSON parser used to validate the /api contract.
struct JsonPair { std::string key, raw, type; };   // type: s(tring) n(umber) b(ool) z(null)

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
  {"pumpAuto", 'b'}, {"lightAuto", 'b'}, {"rgbAuto", 'b'}, {"fanAuto", 'b'}, {"breatheAuto", 'b'}
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

// Full contract validation of the last /api response
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

// D5: strict /api JSON contract
static void testApiJsonContract() {
  printf("D5  api: strict JSON contract (21 keys)\n");
  std::string err;

  // Known state
  gTemp = 26.5f; gHum = 61.2f;
  gLight = 800; gSoil = 650; gWater = 900;
  gPir = true; gTouch = false;
  invoke("/api");
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());

  // Demo state: demo flag + step string must be live
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

  // Feed state
  startFeed();
  invoke("/api");
  err.clear();
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());
  CHECK(has(mockServerLastBody(), "\"feed\":true"));
  stopFeed();

  // 200 randomized states: JSON must stay valid and consistent
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

// D6: HTTP response exactness + threshold edges
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
  CHECK(!lightAuto);                       // only "true" enables auto

  invoke("/rgb", "r", "0", "g", "255", "b", "255");
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbG, 255); CHECK_EQ(rgbB, 255);
  CHECK_STR(mockServerLastBody(), "ok");

  invoke("/breathe");
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK(breatheLedState);
  CHECK(!breatheAuto);
  invoke("/breathe");
  CHECK(!breatheLedState);

  invoke("/windmill");                     // no arg -> off
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
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);        // best possible fix at the edge
  // Equal thresholds must not make the pump chatter (control guard):
  gWater = 900; gSoil = 100; relayState = false; pumpAuto = true;
  controlPump();
  CHECK(!relayState);
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "soilLow", "-5");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);
  CHECK_NEAR(soilThresholdHigh, 700, 0.01);         // untouched (still > low)
  invoke("/threshold", "soilLow", "abc");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);            // toFloat("abc") == 0
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "waterLow", "99999");
  CHECK_NEAR(waterThresholdLow, 4095, 0.01);
  waterThresholdLow = 200;

  invoke("/threshold", "tempLow", "60");
  CHECK_NEAR(tempThresholdLow, 60, 0.01);
  CHECK_NEAR(tempThresholdHigh, 61, 0.01);          // kept strictly above low
  invoke("/threshold", "tempLow", "25");
  CHECK_NEAR(tempThresholdLow, 25, 0.01);
  invoke("/threshold", "tempHigh", "0");
  CHECK_NEAR(tempThresholdHigh, 26, 0.01);          // fixed to low + 1
  tempThresholdHigh = 30; tempThresholdLow = 25;
  invoke("/threshold", "light", "5000");
  CHECK_EQ(lightThreshold, 4095);
  lightThreshold = 500;

  invoke("/rgb");                                    // no args -> zeros
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

// D7: OLED per-row layout + line length limit
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

  // Worst-case values: every row must still fit 21 characters (128 px / 6 px)
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

// D8: serial edge cases + debug-mode behaviour
static void testSerialEdges() {
  printf("D8  serial: edge cases + debug mode\n");

  mockSerialClear();
  mockSerialPush("\r\n");                    // empty line: ignored
  loop();
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  mockSerialClear();
  mockSerialPush("\r");                      // lone CR: ignored
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  char badByte[2] = {(char)0xFF, 0};         // garbage byte: rejected
  mockSerialClear();
  mockSerialPush(badByte);
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));

  mockSerialClear();
  for (int i = 0; i < 64; i++) mockSerialPush("x");   // long garbage line
  for (int i = 0; i < 64; i++) loop();
  CHECK_EQ(mockSerialPending(), 0);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "Invalid"), 64);

  // '0' in normal mode is handled gracefully (prints exit notice, stays normal)
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), ">> Exit debug."));

  // Debug mode pauses the automation (service mode)...
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
  CHECK(relayState);                          // debug mode: automation paused
  // ...and the very first loop after exiting restores the safety rule
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), "Running normally..."));
  CHECK(!relayState);
  resetState();
}

// D9: demo diagnostics (order, duration, OLED names, mid-step safety)
static const char *DEMO_STEPS[] = {
  "HD2: Thap sang", "HD3: Den tho PWM", "HD4: Cham LED (bat)", "HD4: Cham LED (tat)",
  "HD5: PIR co nguoi", "HD5: Nguoi roi di", "HD6: Coi xay gio quay", "HD7: Tuoi tu dong",
  "HD8: Bao nuoc can", "HD9: Den tang truong", "HD10: Cho an tu dong", "HD10: Dong cua",
  "HD11: Xem OLED", "HD12: Dieu khien", "HD12: Tat tu xa"
};
static const int DEMO_STEP_COUNT = (int)(sizeof(DEMO_STEPS) / sizeof(DEMO_STEPS[0]));

static void testDemoDiagnostics() {
  printf("D9  demo: step order, duration, OLED names, mid-step safety\n");

  // Pass 1: exact step sequence + total duration + OLED names
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
  CHECK(elapsed >= 30700 && elapsed <= 30800);       // exact sum of step times
  CHECK_EQ(countOccurrences(mockSerialOutput(), "[DEMO] "), DEMO_STEP_COUNT);
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  CHECK(!relayState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);

  // Pass 2: water runs out mid pump step -> safety must cut the pump at once
  resetState();
  primeSensors(900);
  startDemo();
  bool dropped = false, violated = false, sawPumpOn = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    if (!dropped && strstr(demoStepName(), "HD7")) {
      dropped = true;
      gWater = 50;                                     // tank empties mid-step
      mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
      lastSensorRead = mockMillis();
    }
    simLoop();
    if (relayState) sawPumpOn = true;
    if (relayState && gWater < (int)waterThresholdLow) violated = true;
  }
  CHECK(dropped);
  CHECK(sawPumpOn);                                    // it ran while water was OK
  CHECK(!violated);                                    // then never dry
  CHECK(!demoRunning());
  CHECK(!relayState);
}

// D10: WiFi reconnect throttle counters
static void testWifiThrottle() {
  printf("D10 wifi: reconnect throttle + counters\n");
  mockWifiStatus = WL_CONNECTED;
  const int c0 = mockWifiBeginCalls;
  loop();                                              // resets the warned flag
  CHECK_EQ(mockWifiBeginCalls, c0);                    // connected: no reconnect attempt

  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis();
  const int b0 = mockWifiBeginCalls, d0 = mockWifiDisconnectCalls;
  mockSerialClear();

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                    // not yet due
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);                // due at exactly the interval
  CHECK_EQ(mockWifiDisconnectCalls, d0 + 1);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // retries every 10 s
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);  // but logs once

  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // no extra attempt

  // A new outage logs again (warned flag was reset by the reconnect)
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);
  mockWifiStatus = WL_CONNECTED;
  loop();
}

// D11: boot glitch order + watchdog init
static void testBootOrder() {
  printf("D11 boot: output latch before pinMode + WDT\n");
  mockReset();
  mockServerReset();
  mockIoLogClear();
  setup();
  checkRelayPin(false);                                // pump OFF at boot

  const char *log = mockIoLog();
  const char *wRelay = strstr(log, "W:26=");
  const char *mRelay = strstr(log, "M:26;");
  CHECK(wRelay && mRelay);
  CHECK(wRelay < mRelay);                              // latch written first

  const char *wFan = strstr(log, "W:14=");
  const char *mFan = strstr(log, "M:14;");
  CHECK(wFan && mFan && wFan < mFan);

  const char *wBuzz = strstr(log, "W:25=");
  const char *mBuzz = strstr(log, "M:25;");
  CHECK(wBuzz && mBuzz && wBuzz < mBuzz);

  CHECK(mockWdtInitCalls >= 1);
  CHECK_EQ(mockWdtAddCalls, 1);
  CHECK_EQ(mockServerRouteCount(), 14);
}

// D12: settings survive a simulated power cycle
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

  // power cycle: flash keeps prefs, RAM-only state is lost
  mockServerReset();
  fanState = relayState = windmillOn = blueLedState = breatheLedState = false;
  rgbR = rgbG = rgbB = 0;
  setup();

  CHECK_EQ(mockServerRouteCount(), 14);
  CHECK_NEAR(soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(soilThresholdHigh, 1284, 0.01);
  CHECK(!pumpAuto);
  CHECK(!fanAuto);
  CHECK(breatheAuto);
  CHECK(breatheLedState);                              // setup re-arms breathing
  CHECK(!fanState);                                    // runtime states reset
  CHECK_EQ(rgbR, 0);                                   // manual RGB is not persisted
}

// D13: seeded fuzz/soak with safety invariants
static void checkFuzzInvariants(bool demoAtLoopStart) {
  // 1. The pump must never run with low water (safety line + controlPump).
  CHECK(!(gWater < (int)waterThresholdLow && relayState));
  // 2. Pin levels always match the logical states.
  CHECK((mockPinLevel[RELAY_PIN] != 0) == (relayState != (bool)RELAY_ACTIVE_LOW));
  CHECK((mockPinLevel[FAN_PIN] != 0) == fanState);
  // 3. The shared servo mechanisms never run at the same time.
  CHECK(!(windmillOn && feedRunning()));
  // 4. Blue LED modes are mutually exclusive.
  CHECK(!(blueLedState && breatheLedState));
  // 5. Servo and PWM writes always stay in range.
  CHECK(mockServoMinSeen >= 0 && mockServoMaxSeen <= 180);
  CHECK_EQ(mockInvalidPixelWrites, 0);
  CHECK_EQ(mockInvalidPwmWrites, 0);
  // 6. No stale PWM while both LED modes are off.
  if (!blueLedState && !breatheLedState) CHECK_EQ(currentLedPwm(), 0);
  // 7. While the buzzer is alarmed (non-demo), it must actually be on.
  // Skip the loop in which a running demo just ended: automation (and the
  // alarm) only resumes on the next loop.
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
      case 0: {  // random environment
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
      case 1: {  // random HTTP request with random args
        int na = (int)(rng() % 4);
        for (int a = 0; a < na; a++) {
          mockServerSetArg(argNames[rng() % nNames], argVals[rng() % nVals]);
        }
        bool found = mockServerInvoke(routes[rng() % nRoutes]);
        CHECK(found);
        int code = mockServerLastCode();
        CHECK(code == 200 || code == 204 || code == 400);
        break;
      }
      case 2:  // random serial input
        if (mockSerialPending() < 8) {
          char c[2] = {serialChars[rng() % (int)sizeof(serialChars)], 0};
          mockSerialPush(c);
        }
        break;
      case 3:  // manual actuator toggles
        switch (rng() % 4) {
          case 0: setRelay(!relayState); break;
          case 1: setFan(!fanState); break;
          case 2: setBlueLed(!blueLedState); break;
          case 3: setBreatheLed(!breatheLedState); break;
        }
        break;
      case 4:  // shared servo mechanisms
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
        mockServerInvoke("/demo");
        break;
      case 7:
        mockDigital[TOUCH_PIN] = (rng() % 2) ? HIGH : LOW;
        mockDigital[PIR_PIN] = (rng() % 2) ? HIGH : LOW;
        break;
      case 8: {  // random threshold update
        char v[16];
        snprintf(v, sizeof(v), "%d", (int)(rng() % 5000) - 100);
        mockServerSetArg(argNames[6 + rng() % 6], v);
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

    if (i % 1000 == 999) mockSerialClear();   // bound the capture buffer
  }
}

static void testFuzzSoak() {
  printf("D13 fuzz: seeded soak with safety invariants\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;
  mockServerReset();
  setup();                                   // routes + defaults from "flash"

  fuzzRun(1,    30000);
  fuzzRun(42,   30000);
  fuzzRun(2026, 30000);

  // settle: let any running demo finish, then verify the quiet state
  stopFeed();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); loop(); }
  CHECK(!demoRunning());
  mockSerialClear();
}

// D14: millis() wrap-around safety
static void testClockWrap() {
  printf("D14 clock: millis() wrap-around\n");
  stopFeed();
  setWindmill(false);

  // A: periodic sensor reads across the wrap
  resetState();
  mockSetMillis(0xFFFFFF00UL);
  mockDhtTemp = 41.0f; mockDhtHum = 33.0f;
  mockAdc[LDR_PIN] = 1500; mockAdc[SOIL_PIN] = 800; mockAdc[WATER_PIN] = 1200;
  lastSensorRead = mockMillis();
  runLoop(5100);                             // crosses the wrap at +256 ms
  CHECK_NEAR(gTemp, 41.0, 0.01);
  CHECK_NEAR(gHum, 33.0, 0.01);

  // B: feed timer spanning the wrap
  mockSetMillis(0xFFFFFFFFUL);
  startFeed();
  CHECK(feedRunning());
  mockAdvance(1);                            // wraps to 0
  mockAdvance(1998);
  controlFeed();
  CHECK(feedRunning());                      // 1999 ms < FEED_DURATION
  mockAdvance(2);
  controlFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  // C: breathe timer spanning the wrap
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

  // D: touch toggle with a large timestamp
  mockSetMillis(0xF0000000UL);
  mockAdvance(10000);
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  bool before = blueLedState;
  mockAdvance(TOUCH_DEBOUNCE);               // clear the debounce window first
  gTouch = true; controlTouch();
  CHECK(blueLedState != before);
  gTouch = false; controlTouch();
  setBlueLed(false);

  // E: the full demo must finish cleanly when the wrap happens mid-run
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

  // F: WiFi retry timer spanning the wrap
  mockSetMillis(0xFFFFFF00UL);
  mockWifiStatus = WL_CONNECTED;
  loop();                                            // clears the warned flag
  int b0 = mockWifiBeginCalls;
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = 0xFFFFFF00UL;
  mockAdvance(9999);                                  // wraps past the boundary
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                   // 9999 ms: not yet due
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);               // 10000 ms: due
  mockWifiStatus = WL_CONNECTED;
  loop();

  resetState();
}

// ==============================================
//  Main
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

  printf("=== %d checks, %d failures ===\n", gChecks, gFails);
  return gFails ? 1 : 0;
}
