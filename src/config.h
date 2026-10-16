#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ── WiFi ──
extern const char *ssid;
extern const char *password;

// ── GPIO Pin Map ──
#define DHT_PIN          23
#define DHT_TYPE         DHT11
#define RGB_PIN          19
#define RGB_COUNT        4
#define SERVO_PIN        13
#define BUZZER_PIN       25
#define RELAY_PIN        26
#define FAN_PIN          14
#define BLUE_LED_PIN     27
#define LED_PWM_CHANNEL  15      // LEDC channel for blue LED PWM (core 2.x); avoids servo timer conflict
#define TOUCH_PIN        39
#define PIR_PIN          33
#define LDR_PIN          34
#define SOIL_PIN         35
#define WATER_PIN        36
#define OLED_SDA         21
#define OLED_SCL         22
#define OLED_RESET       -1
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64

// ── Control Thresholds ──
extern float soilThresholdLow;
extern float soilThresholdHigh;
extern float waterThresholdLow;
extern int   lightThreshold;
extern float tempThresholdHigh;
extern float tempThresholdLow;

// ── Timing (ms) ──
extern const unsigned long WIFI_CONNECT_TIMEOUT;
extern const unsigned long WIFI_CHECK_INTERVAL;
extern const unsigned long SENSOR_INTERVAL;
extern const unsigned long OLED_INTERVAL;
extern const unsigned long TOUCH_DEBOUNCE;
extern const unsigned long BREATHE_INTERVAL;

// ── Sensor Filtering ──
constexpr uint8_t SENSOR_FILTER_SIZE = 5;  // Moving average window

#endif
