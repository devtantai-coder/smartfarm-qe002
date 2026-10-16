#ifndef GLOBALS_H
#define GLOBALS_H

#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <Preferences.h>
#include "logger.h"

// ── System State ──
extern bool pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto;
extern bool relayState, fanState, blueLedState, breatheLedState;
extern uint8_t rgbR, rgbG, rgbB;
extern uint8_t breatheBrightness;
extern int8_t breatheDirection;
extern int servoAngle;

// ── Sensor Data ──
extern float gTemp, gHum;
extern int gLight, gSoil, gWater;
extern bool gPir, gTouch;

// ── Timing ──
extern unsigned long lastWifiCheck, lastSensorRead, lastOledUpdate;
extern unsigned long lastBuzzer, lastTouch, lastBreatheUpdate;

// ── Hardware Objects ──
extern DHT dht;
extern Adafruit_SSD1306 display;
extern Adafruit_NeoPixel strip;
extern Servo servo;
extern WebServer server;
extern Preferences preferences;

// ── Sensor Health ──
extern uint8_t sensorFailCount;
extern bool sensorHealthy;

#endif
