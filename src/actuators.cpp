#include "globals.h"
#include "logger.h"
#include "actuators.h"

void setRelay(bool on) {
  relayState = on;
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Pump: ON" : "Pump: OFF");
}

void setFan(bool on) {
  fanState = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Fan: ON" : "Fan: OFF");
}

void setBlueLed(bool on) {
  blueLedState = on;
  digitalWrite(BLUE_LED_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Blue LED: ON" : "Blue LED: OFF");
}

void setBreatheLed(bool on) {
  breatheLedState = on;
  if (!on) {
    ledcWrite(BREATHE_LED_CHANNEL, 0);
    breatheBrightness = 0;
    breatheDirection = 1;
    LOG_INFO("Breathe: OFF");
  } else {
    LOG_INFO("Breathe: ON");
  }
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
