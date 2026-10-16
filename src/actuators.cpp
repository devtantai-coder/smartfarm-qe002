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

// Blue LED (pin 27) is driven by PWM so both the steady on/off mode and the
// breathing effect share the same hardware. The two modes are mutually
// exclusive (blueLedState vs breatheLedState); a steady command wins.
void setBlueLed(bool on) {
  if (on) {
    if (blueLedState && !breatheLedState) return;
    blueLedState = true;
    breatheLedState = false;    // steady light wins over breathing
    breatheBrightness = 0;
    setLedPwm(255);
  } else {
    if (!blueLedState && !breatheLedState) return;
    blueLedState = false;
    breatheLedState = false;
    breatheBrightness = 0;
    setLedPwm(0);
  }
  LOG_INFO(on ? "Blue LED: ON" : "Blue LED: OFF");
}

// Single place that talks to the PWM peripheral (core 2.x uses a channel,
// core 3.x uses the pin).
void setLedPwm(uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BLUE_LED_PIN, value);
#else
  ledcWrite(LED_PWM_CHANNEL, value);
#endif
}

void setBreatheLed(bool on) {
  if (breatheLedState == on) return;
  breatheLedState = on;
  breatheBrightness = 0;
  breatheDirection = 1;
  if (on) {
    blueLedState = false;       // breathing takes over the blue LED
  } else {
    setLedPwm(0);
  }
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

// Plant growth light (activity 9): LED 1 & 3 red, LED 2 & 4 blue.
void setRgbPlant() {
  rgbR = 255; rgbG = 0; rgbB = 255;
  for (int i = 0; i < RGB_COUNT; i++) {
    bool red = (i % 2 == 0);
    strip.setPixelColor(i, red ? strip.Color(255, 0, 0) : strip.Color(0, 0, 255));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB plant light");
}

void setRgbAuto(bool on)  { rgbAuto = on; }
void setPumpAuto(bool on) { pumpAuto = on; }
void setFanAuto(bool on)  { fanAuto = on; }
void setLightAuto(bool on){ lightAuto = on; }
void setBreatheAuto(bool on) { breatheAuto = on; }
