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

// ── Tương thích phiên bản core ESP32 (2.x và 3.x) ──
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

#include "config.h"
#include "logger.h"

// ── Trạng thái hệ thống ──
extern bool pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto;
extern bool relayState, fanState, blueLedState, breatheLedState;
extern bool windmillOn;
extern uint8_t rgbR, rgbG, rgbB;
extern uint8_t breatheBrightness;
extern int8_t breatheDirection;
extern int servoAngle;

// ── Dữ liệu cảm biến ──
extern float gTemp, gHum;
extern int gLight, gSoil, gWater;
extern bool gPir, gTouch;

// ── Thời gian ──
extern unsigned long lastWifiCheck, lastSensorRead, lastOledUpdate;
extern unsigned long lastBuzzer, lastTouch, lastBreatheUpdate;

// ── Đối tượng phần cứng ──
extern DHT dht;
extern Adafruit_SSD1306 display;
extern Adafruit_NeoPixel strip;
extern Servo servo;
extern WebServer server;
extern Preferences preferences;

// ── Sức khỏe cảm biến ──
extern uint8_t sensorFailCount;
extern bool sensorHealthy;

// ── Mã bảo mật (lưu trong Preferences, fallback về compile-time) ──
extern char webToken[64];
extern char otaPassword[64];

// ── Chống replay ──
extern uint32_t gLastNonce;

// ── Cờ khởi động lại sau khi lưu cài đặt ──
extern bool gNeedRestart;

#endif
