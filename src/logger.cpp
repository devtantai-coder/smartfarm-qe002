#include "logger.h"
#include "globals.h"

// ── Module debug flags ──
bool dbgSensors = false, dbgActuators = false, dbgControl = false;
bool dbgWebserver = false, dbgWifi = false, dbgOled = false;
bool dbgSettings = false, dbgLogger = false, dbgRgb = false;

static LogLevel sMinLevel = LOG_INFO;
static bool sInit = false;

static const char* levelTag(LogLevel l) {
  switch (l) {
    case LOG_DEBUG: return "DBG";
    case LOG_INFO:  return "INF";
    case LOG_WARNING: return "WRN";
    case LOG_ERROR: return "ERR";
    case LOG_FATAL: return "FTL";
    default: return "???";
  }
}

void logMessage(LogLevel level, const char* func, int line, const char* msg) {
  if (level < sMinLevel) return;
  if (!sInit) {
    Serial.println(F("\n=== SMART FARM LOGGER ==="));
    sInit = true;
  }
  Serial.print('[');
  Serial.print(levelTag(level));
  Serial.print(F("] "));
  Serial.print(func);
  Serial.print(':');
  Serial.print(line);
  Serial.print(F(" > "));
  Serial.println(msg);
}

void logError(LogLevel level, const char* func, int line, int code, const char* msg) {
  logMessage(level, func, line, msg);
  Serial.print(F("   [CODE:"));
  Serial.print(code);
  Serial.println(']');
}

// ── Debug: Sensors ──
void debugSensors() {
  Serial.println(F("\n--- SENSORS ---"));
  readAllSensors();
  Serial.printf("T:%.1f°C  H:%.1f%%\n", gTemp, gHum);
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
  Serial.println(F("Servo sweep..."));
  servo.write(0); delay(600);
  servo.write(90); delay(600);
  servo.write(180); delay(600);
  servo.write(0);
}

// ── Debug: Control ──
void debugControl() {
  Serial.println(F("\n--- CONTROL ---"));
  Serial.printf("Soil: %.0f-%.0f  Water:%.0f\n", soilThresholdLow, soilThresholdHigh, waterThresholdLow);
  Serial.printf("Light:%d  Temp:%.0f-%.0f\n", lightThreshold, tempThresholdLow, tempThresholdHigh);
  Serial.printf("Auto: P:%d L:%d R:%d F:%d B:%d\n", pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto);
}

// ── Debug: Webserver ──
void debugWebserver() {
  Serial.println(F("\n--- WEBSERVER ---"));
  Serial.printf("WiFi:%s  IP:%s\n", WiFi.status()==WL_CONNECTED?"OK":"FAIL", WiFi.localIP().toString().c_str());
  Serial.println(F("Routes: / /api /toggle /auto /rgb /servo /breathe"));
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
  display.setCursor(0,60); display.print(F("Done"));
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
