#include "config.h"

// ── WiFi ──
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";

// ── Thresholds ──
float soilThresholdLow  = 400.0f;
float soilThresholdHigh = 700.0f;
float waterThresholdLow = 200.0f;
int   lightThreshold    = 500;
float tempThresholdHigh = 30.0f;
float tempThresholdLow  = 25.0f;

// ── Timing ──
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long SENSOR_INTERVAL     = 1000;
const unsigned long OLED_INTERVAL       = 2000;
const unsigned long TOUCH_DEBOUNCE      = 300;
const unsigned long BREATHE_INTERVAL    = 30;


