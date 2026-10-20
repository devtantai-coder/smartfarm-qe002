#include "logger.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"
#include "oled.h"

// ── Các cờ debug theo module ──
bool dbgSensors = false, dbgActuators = false, dbgControl = false;
bool dbgWebserver = false, dbgWifi = false, dbgOled = false;
bool dbgSettings = false, dbgLogger = false, dbgRgb = false;

// Mức tối thiểu được in (LOG_DEBUG hiển thị mọi thứ)
LogLevel gMinLogLevel = LOG_INFO;

static bool sLogInit = false;

static const char* levelTag(LogLevel l) {
  switch (l) {
    case LOG_DEBUG:   return "DEBUG";
    case LOG_INFO:    return "INFO ";
    case LOG_WARNING: return "WARN ";
    case LOG_ERROR:   return "ERROR";
    case LOG_FATAL:   return "FATAL";
    default:          return "?????";
  }
}

static void ensureInit() {
  if (sLogInit) return;
  sLogInit = true;
  Serial.println(F("\n=== SMART FARM LOGGER ==="));
}

// Tiền tố [HH:MM:SS.mmm] [LEVEL] hàm:dòng — ghi vào buffer để mỗi dòng log
// chỉ tốn MỘT lần Serial.print (trước đây 5-6 lần, mỗi lần đều khóa UART).
static size_t formatPrefix(char* buf, size_t n, LogLevel level, const char* func, int line) {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  return (size_t)snprintf(buf, n, "[%02lu:%02lu:%02lu.%03lu] [%s] %s:%d",
                          (sec / 3600) % 100, (sec / 60) % 60, sec % 60, ms % 1000,
                          levelTag(level), func, line);
}

void logMessage(LogLevel level, const char* func, int line, const char* msg) {
  if (level < gMinLogLevel) return;
  ensureInit();
  char buf[192];
  size_t n = formatPrefix(buf, sizeof(buf), level, func, line);
  snprintf(buf + n, sizeof(buf) - n, " -> %s\n", msg);
  Serial.print(buf);
}

void logError(LogLevel level, const char* func, int line, int code, const char* msg) {
  if (level < gMinLogLevel) return;  // khắc phục lỗi mã code vẫn bị in khi dưới mức tối thiểu
  ensureInit();
  char buf[192];
  size_t n = formatPrefix(buf, sizeof(buf), level, func, line);
  snprintf(buf + n, sizeof(buf) - n, " [MA LOI: %d] -> %s\n", code, msg);
  Serial.print(buf);
}

// ── Dòng cảm biến định kỳ (activity 7 / 11): hiển thị ở mức log DEBUG ──
void logSensors() {
  if (LOG_DEBUG < gMinLogLevel) return;
  Serial.printf("[SENSORS] T:%.1fC H:%.1f%% Light:%d Soil:%d Water:%d PIR:%d Touch:%d\n",
                sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater, (int)sys.gPir, (int)sys.gTouch);
}

// ── Debug: Cảm biến ──
void debugSensors() {
  Serial.println(F("\n--- SENSORS ---"));
  readAllSensors();
  Serial.printf("T:%.1fC  H:%.1f%%\n", sys.gTemp, sys.gHum);
  Serial.printf("Light:%d  Soil:%d  Water:%d\n", sys.gLight, sys.gSoil, sys.gWater);
  Serial.printf("PIR:%d  Touch:%d  Health:%d\n", sys.gPir, sys.gTouch, sys.sensorHealthy);
}

// ── Debug: Cơ cấu chấp hành (KHÔNG chặn) ──
// Bấm '2' chỉ BẮT ĐẦU trình tự; loop() gọi updateDebugActuators() mỗi vòng
// (kể cả khi đang ở chế độ debug) để chạy tiếp, nên web server/OTA vẫn phục
// vụ bình thường trong lúc test thay vì bị delay() chặn như trước.
enum DebugActAction {
  DAA_RELAY_ON, DAA_RELAY_OFF, DAA_FAN_ON, DAA_FAN_OFF,
  DAA_LED_ON, DAA_LED_OFF, DAA_BREATHE_ON, DAA_BREATHE_OFF,
  DAA_RGB_RED, DAA_RGB_GREEN, DAA_RGB_BLUE, DAA_RGB_OFF,
  DAA_PLANT, DAA_BUZZER_ON, DAA_BUZZER_OFF,
  DAA_SERVO_MIN, DAA_SERVO_MID, DAA_SERVO_MAX
};

struct DebugActStep {
  DebugActAction act;
  uint16_t ms;          // thời gian giữ bước trước khi sang bước kế (0 = ngay)
  const char* label;    // in ra khi bước bắt đầu (nullptr = không in)
};

static const DebugActStep DEBUG_ACT_STEPS[] = {
  { DAA_RELAY_ON,    800,  "Pump on/off..." },
  { DAA_RELAY_OFF,   400,  nullptr },
  { DAA_FAN_ON,      800,  "Fan on/off..." },
  { DAA_FAN_OFF,     400,  nullptr },
  { DAA_LED_ON,      800,  "LED on/off..." },
  { DAA_LED_OFF,     400,  nullptr },
  { DAA_BREATHE_ON,  1500, "Breathe on/off..." },
  { DAA_BREATHE_OFF, 400,  nullptr },
  { DAA_RGB_RED,     600,  "RGB test..." },
  { DAA_RGB_GREEN,   600,  nullptr },
  { DAA_RGB_BLUE,    600,  nullptr },
  { DAA_RGB_OFF,     0,    nullptr },
  { DAA_PLANT,       800,  "RGB plant pattern..." },
  { DAA_RGB_OFF,     0,    nullptr },
  { DAA_BUZZER_ON,   400,  "Buzzer on/off..." },
  { DAA_BUZZER_OFF,  0,    nullptr },
  { DAA_SERVO_MIN,   600,  "Servo sweep..." },
  { DAA_SERVO_MID,   600,  nullptr },
  { DAA_SERVO_MAX,   600,  nullptr },
  { DAA_SERVO_MIN,   0,    nullptr },
};
static const uint8_t DEBUG_ACT_STEP_COUNT = sizeof(DEBUG_ACT_STEPS) / sizeof(DEBUG_ACT_STEPS[0]);

static bool sActSeqActive = false;
static uint8_t sActSeqStep = 0;
static unsigned long sActSeqStart = 0;

static void runDebugActAction(DebugActAction a) {
  switch (a) {
    case DAA_RELAY_ON:    setRelay(true); break;
    case DAA_RELAY_OFF:   setRelay(false); break;
    case DAA_FAN_ON:      setFan(true); break;
    case DAA_FAN_OFF:     setFan(false); break;
    case DAA_LED_ON:      setBlueLed(true); break;
    case DAA_LED_OFF:     setBlueLed(false); break;
    case DAA_BREATHE_ON:  setBreatheLed(true); break;
    case DAA_BREATHE_OFF: setBreatheLed(false); break;
    case DAA_RGB_RED:     setRgb(255, 0, 0); break;
    case DAA_RGB_GREEN:   setRgb(0, 255, 0); break;
    case DAA_RGB_BLUE:    setRgb(0, 0, 255); break;
    case DAA_RGB_OFF:     setRgb(0, 0, 0); break;
    case DAA_PLANT:       setRgbPlant(); break;
    case DAA_BUZZER_ON:   setBuzzer(true); break;
    case DAA_BUZZER_OFF:  setBuzzer(false); break;
    case DAA_SERVO_MIN:   servo.write(WINDMILL_MIN_ANGLE); break;
    case DAA_SERVO_MID:   servo.write((WINDMILL_MIN_ANGLE + WINDMILL_MAX_ANGLE) / 2); break;
    case DAA_SERVO_MAX:   servo.write(WINDMILL_MAX_ANGLE); break;
  }
}

static void startDebugActStep(uint8_t i) {
  if (DEBUG_ACT_STEPS[i].label) Serial.println(DEBUG_ACT_STEPS[i].label);
  runDebugActAction(DEBUG_ACT_STEPS[i].act);
}

void debugActuators() {
  Serial.println(F("\n--- ACTUATORS ---"));
  sActSeqActive = true;
  sActSeqStep = 0;
  sActSeqStart = millis();
  startDebugActStep(0);
}

void updateDebugActuators() {
  if (!sActSeqActive) return;
  unsigned long now = millis();
  // Có thể hoàn tất nhiều bước hết hạn (kể cả bước 0 ms) trong cùng một lần gọi
  while (sActSeqActive && now - sActSeqStart >= DEBUG_ACT_STEPS[sActSeqStep].ms) {
    sActSeqStep++;
    if (sActSeqStep >= DEBUG_ACT_STEP_COUNT) {
      sActSeqActive = false;
      return;
    }
    sActSeqStart = now;
    startDebugActStep(sActSeqStep);
  }
}

// Thoát debug giữa chừng: dừng trình tự và trả cơ cấu về trạng thái an toàn.
void stopDebugActuators() {
  if (!sActSeqActive) return;
  sActSeqActive = false;
  setRelay(false);
  setFan(false);
  setBlueLed(false);
  setBreatheLed(false);
  setBuzzer(false);
  setRgb(0, 0, 0);
  servo.write(WINDMILL_MIN_ANGLE);
  Serial.println(F(">> Actuator test stopped."));
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: đưa trình tự debug cơ cấu về trạng thái vừa cấp điện.
void testResetLoggerStatics() {
  sActSeqActive = false;
  sActSeqStep = 0;
  sActSeqStart = 0;
}
#endif

// ── Debug: Điều khiển ──
void debugControl() {
  Serial.println(F("\n--- CONTROL ---"));
  Serial.printf("Soil: %.0f-%.0f  Water:%.0f\n", sys.soilThresholdLow, sys.soilThresholdHigh, sys.waterThresholdLow);
  Serial.printf("Light:%d  Temp:%.0f-%.0f\n", sys.lightThreshold, sys.tempThresholdLow, sys.tempThresholdHigh);
  Serial.printf("Auto: P:%d L:%d R:%d F:%d B:%d\n", sys.pumpAuto, sys.lightAuto, sys.rgbAuto, sys.fanAuto, sys.breatheAuto);
  Serial.printf("Windmill:%d Servo:%d\n", sys.windmillOn, sys.servoAngle);
}

// ── Debug: Webserver ──
void debugWebserver() {
  Serial.println(F("\n--- WEBSERVER ---"));
  Serial.printf("WiFi:%s  IP:%s\n", WiFi.status()==WL_CONNECTED?"OK":"FAIL", WiFi.localIP().toString().c_str());
  Serial.println(F("Routes: / /api /toggle /auto /rgb /breathe /on /off /windmill /feed /demo /servo /threshold"));
}

// ── Debug: WiFi ──
void debugWifi() {
  Serial.println(F("\n--- WIFI ---"));
  Serial.printf("SSID:%s  Status:%d\n", sys.ssid, WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("IP:%s  MAC:%s  RSSI:%d\n", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), WiFi.RSSI());
  } else {
    WiFi.reconnect();
  }
}

// ── Debug: OLED ──
void debugOled() {
  Serial.println(F("\n--- OLED ---"));
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print(F("DEBUG OLED"));
  display.setCursor(0,16); display.print(F("Line 1"));
  display.setCursor(0,32); display.print(F("Line 2"));
  display.setCursor(0,48); display.print(F("Line 3"));
  display.display();
  oledForceRedraw();   // lần cập nhật kế tiếp đẩy lại cả khung, không sót mẫu test
  Serial.println(F("OLED test pattern sent."));
}

// ── Debug: Cài đặt ──
void debugSettings() {
  Serial.println(F("\n--- SETTINGS ---"));
  Serial.printf("soilLow:%.0f soilHigh:%.0f\n", sys.soilThresholdLow, sys.soilThresholdHigh);
  Serial.printf("waterLow:%.0f lightThr:%d\n", sys.waterThresholdLow, sys.lightThreshold);
  Serial.printf("tempHigh:%.0f tempLow:%.0f\n", sys.tempThresholdHigh, sys.tempThresholdLow);
  Serial.printf("pump:%d light:%d rgb:%d fan:%d breathe:%d\n", sys.pumpAuto, sys.lightAuto, sys.rgbAuto, sys.fanAuto, sys.breatheAuto);
}

// ── Debug: Logger ──
void debugLogger() {
  Serial.println(F("\n--- LOGGER ---"));
  LOG_DEBUG("Debug message");
  LOG_INFO("Info message");
  LOG_WARN("Warning message");
  LOG_ERROR_CODE(999, "Error with code");
  LOG_FATAL("Fatal message");
}
