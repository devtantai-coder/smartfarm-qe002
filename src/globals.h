#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

#if defined(BLUETOOTH_ENABLED) && BLUETOOTH_ENABLED
#include <BluetoothSerial.h>
extern BluetoothSerial SerialBT;
#endif

// ── Tương thích phiên bản core ESP32 (2.x và 3.x) ──
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "logger.h"

// ── Trạng thái toàn cục ──
// Mọi biến mutable được gom vào một struct duy nhất (định nghĩa ở globals.cpp)
// để rõ chủ sở hữu, dễ reset và tránh biến rời rạc rải khắp chương trình.
struct SystemState {
  // Chế độ tự động
  bool pumpAuto = true, lightAuto = true, rgbAuto = true, fanAuto = true, breatheAuto = false;

  // Trạng thái cơ cấu chấp hành
  bool relayState = false, fanState = false, blueLedState = false, breatheLedState = false;
  bool windmillOn = false;
  uint8_t rgbR = 0, rgbG = 0, rgbB = 0;
  uint8_t breatheBrightness = 0;
  int8_t breatheDirection = 1;
  int servoAngle = 0;

  // Dữ liệu cảm biến (giá trị đã lọc)
  float gTemp = 0, gHum = 0;
  int gLight = 0, gSoil = 0, gWater = 0;
  bool gPir = false, gTouch = false;

  // Ngưỡng điều khiển (người dùng chỉnh được, lưu flash)
  float soilThresholdLow = 400.0f, soilThresholdHigh = 700.0f;
  float waterThresholdLow = 200.0f;
  int   lightThreshold = 500;
  float tempThresholdHigh = 30.0f, tempThresholdLow = 25.0f;

  // Mốc thời gian (millis)
  unsigned long lastWifiCheck = 0, lastSensorRead = 0, lastOledUpdate = 0;
  unsigned long lastBuzzer = 0, lastTouch = 0, lastBreatheUpdate = 0;

  // Sức khỏe cảm biến
  uint8_t sensorFailCount = 0;
  bool sensorHealthy = true;

  // Calibration coefficients
  float calTempOffset = 0.0f, calTempGain = 1.0f;
  float calHumGain = 1.0f;
  int calLightOffset = 0, calSoilOffset = 0, calWaterOffset = 0;

  // PID state for fan
  float pidKp = 1.0f, pidKi = 0.0f, pidKd = 0.0f;
  float pidIntegral = 0.0f, pidPrevError = 0.0f;
  unsigned long pidLastTime = 0;

  // Anomaly scores
  float anomalyTemp = 0.0f, anomalyHum = 0.0f;
  bool sensorHealthyTemp = true, sensorHealthyHum = true;

  // Cờ "cần vẽ lại OLED ngay": đặt khi trạng thái hiển thị trên màn hình vừa
  // đổi (đang quay cối, đang cho ăn, bước demo mới) để màn hình cập nhật
  // ngay ở vòng loop kế tiếp thay vì chờ tới lần vẽ định kỳ 2 giây.
  bool oledDirty = false;

  // Cấu hình kết nối + mã bảo mật (compile-time, KHÔNG ghi flash dạng plaintext)
  char ssid[32] = "Tab S10 Lite của Trinh";
  char password[64] = "99999999T";
  char webToken[64] = WEB_TOKEN;
  char otaPassword[64] = OTA_PASSWORD;

  // Chống replay + cờ khởi động lại
  uint32_t gLastNonce = 0;
  bool gNeedRestart = false;

  // Tiết kiệm điện: khi bật, sau mỗi chu kỳ firmware vào deep sleep theo sleepSeconds
  // và tự tỉnh dậy bằng timer (và chân cảm ứng/chuyển động nếu nối). Mặc định tắt
  // để giữ web server phản hồi thời gian thực.
  bool powerSave = false;
  uint32_t sleepSeconds = 30;
};

extern SystemState sys;

// ── Đối tượng phần cứng ──
extern DHT dht;
extern Adafruit_SSD1306 display;
extern Adafruit_NeoPixel strip;
extern Servo servo;
extern WebServer server;
extern Preferences preferences;

#endif
