#include "globals.h"
#include "logger.h"
#include "actuators.h"

// All setters are idempotent: no re-write / no log spam when the state
// does not change. This keeps controlAlerts()/controlLight() cheap even
// though they are called on every loop iteration.

void setRelay(bool on) {
  if (relayState == on) return;
  relayState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Pump: ON" : "Pump: OFF");
}

void setFan(bool on) {
  if (fanState == on) return;
  fanState = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Fan: ON" : "Fan: OFF");
}

void setBlueLed(bool on) {
  if (blueLedState == on) return;
  blueLedState = on;
  digitalWrite(BLUE_LED_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Blue LED: ON" : "Blue LED: OFF");
}

// Single place that talks to the PWM peripheral (core 2.x uses a channel,
// core 3.x uses the pin).
void setBreathePwm(uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BREATHE_LED_PIN, value);
#else
  ledcWrite(BREATHE_LED_CHANNEL, value);
#endif
}

void setBreatheLed(bool on) {
  if (breatheLedState == on) return;
  breatheLedState = on;
  breatheBrightness = 0;
  breatheDirection = 1;
  if (!on) setBreathePwm(0);
  LOG_INFO(on ? "Breathe: ON" : "Breathe: OFF");
}

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgbR = r; rgbG = g; rgbB = b;
  for (int i = 0; i < RGB_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB set");
}

void setRgbAuto(bool on)  { rgbAuto = on; }
void setPumpAuto(bool on) { pumpAuto = on; }
void setFanAuto(bool on)  { fanAuto = on; }
void setLightAuto(bool on){ lightAuto = on; }
void setBreatheAuto(bool on) { breatheAuto = on; }
