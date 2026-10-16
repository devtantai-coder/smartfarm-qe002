#include "logger.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"

// ── Module debug flags ──
bool dbgSensors = false, dbgActuators = false, dbgControl = false;
bool dbgWebserver = false, dbgWifi = false, dbgOled = false;
bool dbgSettings = false, dbgLogger = false, dbgRgb = false;

// Minimum level printed (LOG_DEBUG shows everything)
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

// [HH:MM:SS.mmm] [LEVEL] func:line
static void printHeader(LogLevel level, const char* func, int line) {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  char stamp[40];
  snprintf(stamp, sizeof(stamp), "[%02lu:%02lu:%02lu.%03lu] [%s] ",
           (sec / 3600) % 100, (sec / 60) % 60, sec % 60, ms % 1000,
           levelTag(level));
  Serial.print(stamp);
  Serial.print(func);
  Serial.print(':');
  Serial.print(line);
}

void logMessage(LogLevel level, const char* func, int line, const char* msg) {
  if (level < gMinLogLevel) return;
  ensureInit();
  printHeader(level, func, line);
  Serial.print(F(" -> "));
  Serial.println(msg);
}

void logError(LogLevel level, const char* func, int line, int code, const char* msg) {
  if (level < gMinLogLevel) return;  // fixes code being printed below min level
  ensureInit();
  printHeader(level, func, line);
  Serial.printf(" [MA LOI: %d] -> %s\n", code, msg);
}

// ── Periodic sensor line (activity 7 / 11): visible at DEBUG log level ──
void logSensors() {
  if (LOG_DEBUG < gMinLogLevel) return;
  Serial.printf("[SENSORS] T:%.1fC H:%.1f%% Light:%d Soil:%d Water:%d PIR:%d Touch:%d\n",
                gTemp, gHum, gLight, gSoil, gWater, (int)gPir, (int)gTouch);
}

// ── Debug: Sensors ──
void debugSensors() {
  Serial.println(F("\n--- SENSORS ---"));
  readAllSensors();
  Serial.printf("T:%.1fC  H:%.1f%%\n", gTemp, gHum);
  Serial.printf("Light:%d  Soil:%d  Water:%d\n", gLight, gSoil, gWater);
  Serial.printf("PIR:%d  Touch:%d  Health:%d\n", gPir, gTouch, sensorHealthy);
}

// ── Debug: Actuators ──
void debugActuators() {
  Serial.println(F("\n--- ACTUATORS ---"));
  Serial.println(F("Pump on/off..."));
  setRelay(true);  delay(800); setRelay(false); delay(400);
  Serial.println(F("Fan on/off..."));
  setFan(true);    delay(800); setFan(false);   delay(400);
  Serial.println(F("LED on/off..."));
  setBlueLed(true); delay(800); setBlueLed(false); delay(400);
  Serial.println(F("Breathe on/off..."));
  setBreatheLed(true); delay(1500); setBreatheLed(false); delay(400);
  Serial.println(F("RGB test..."));
  setRgb(255,0,0); delay(600);
  setRgb(0,255,0); delay(600);
  setRgb(0,0,255); delay(600);
  setRgb(0,0,0);
  Serial.println(F("RGB plant pattern..."));
  setRgbPlant(); delay(800); setRgb(0,0,0);
  Serial.println(F("Buzzer on/off..."));
  setBuzzer(true); delay(400); setBuzzer(false);
  Serial.println(F("Servo sweep..."));
  servo.write(WINDMILL_MIN_ANGLE); delay(600);
  servo.write((WINDMILL_MIN_ANGLE + WINDMILL_MAX_ANGLE) / 2); delay(600);
  servo.write(WINDMILL_MAX_ANGLE); delay(600);
  servo.write(WINDMILL_MIN_ANGLE);
}

// ── Debug: Control ──
void debugControl() {
  Serial.println(F("\n--- CONTROL ---"));
  Serial.printf("Soil: %.0f-%.0f  Water:%.0f\n", soilThresholdLow, soilThresholdHigh, waterThresholdLow);
  Serial.printf("Light:%d  Temp:%.0f-%.0f\n", lightThreshold, tempThresholdLow, tempThresholdHigh);
  Serial.printf("Auto: P:%d L:%d R:%d F:%d B:%d\n", pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto);
  Serial.printf("Windmill:%d Servo:%d\n", windmillOn, servoAngle);
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
  Serial.printf("SSID:%s  Status:%d\n", ssid, WiFi.status());
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
  Serial.println(F("OLED test pattern sent."));
}

// ── Debug: Settings ──
void debugSettings() {
  Serial.println(F("\n--- SETTINGS ---"));
  Serial.printf("soilLow:%.0f soilHigh:%.0f\n", soilThresholdLow, soilThresholdHigh);
  Serial.printf("waterLow:%.0f lightThr:%d\n", waterThresholdLow, lightThreshold);
  Serial.printf("tempHigh:%.0f tempLow:%.0f\n", tempThresholdHigh, tempThresholdLow);
  Serial.printf("pump:%d light:%d rgb:%d fan:%d breathe:%d\n", pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto);
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
