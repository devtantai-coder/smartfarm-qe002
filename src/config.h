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
extern const unsigned long WINDMILL_INTERVAL;
extern const unsigned long FEED_DURATION;

// ── Sensor Filtering ──
constexpr uint8_t SENSOR_FILTER_SIZE = 5;  // Moving average window

// ── Hardware Adaptation (calibrate to your kit) ──
// Documented sensor direction: LDR bright = high, soil dry = low,
// water full = high. If a module reads the opposite way, set the matching
// flag to 1 here instead of changing any control logic.
// (Each flag can also be overridden from the build command line, e.g. -DINVERT_LIGHT=1)
#ifndef INVERT_LIGHT
#define INVERT_LIGHT     0
#endif
#ifndef INVERT_SOIL
#define INVERT_SOIL      0
#endif
#ifndef INVERT_WATER
#define INVERT_WATER     0
#endif

// Pump relay board polarity: 1 = active-LOW module (pump runs on LOW level)
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 0
#endif

// Buzzer type: 1 = passive buzzer (needs tone()), 0 = active buzzer
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE   0
#endif
#ifndef BUZZER_TONE_HZ
#define BUZZER_TONE_HZ   2000
#endif

// Servo angles: calibrate to the mechanical assembly (avoid gear stall)
#ifndef WINDMILL_MIN_ANGLE
#define WINDMILL_MIN_ANGLE 0     // windmill sweep lower limit
#endif
#ifndef WINDMILL_MAX_ANGLE
#define WINDMILL_MAX_ANGLE 180   // windmill sweep upper limit
#endif
#ifndef FEED_OPEN_ANGLE
#define FEED_OPEN_ANGLE    90    // feed gate open position
#endif
#ifndef FEED_CLOSE_ANGLE
#define FEED_CLOSE_ANGLE   0     // feed gate closed position
#endif

#endif
