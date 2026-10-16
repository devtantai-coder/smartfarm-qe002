#include "globals.h"
#include "logger.h"
#include "config.h"
#include "actuators.h"
#include "settings.h"
#include "control.h"

// ── Alert state (edge-triggered logs, no serial spam) ──
static bool sWaterAlarm = false;
static bool sMotionAlarm = false;

// ── Pump: soil moisture + water safety ──
void controlPump() {
  if (!pumpAuto) return;

  // Safety: no water = no pump
  if (gWater < (int)waterThresholdLow) {
    if (relayState) {
      setRelay(false);
      LOG_ERROR_CODE(201, "Low water, pump OFF");
    }
    return;
  }

  // Hysteresis: turn on below low, turn off above high
  if (gSoil < (int)soilThresholdLow && !relayState) {
    setRelay(true);
    LOG_INFO("Soil dry, pump ON [202]");
  } else if (gSoil > (int)soilThresholdHigh && relayState) {
    setRelay(false);
    LOG_INFO("Soil wet, pump OFF [203]");
  }
}

// ── Fan: temperature hysteresis ──
void controlFan() {
  if (!fanAuto) return;
  if (gTemp > tempThresholdHigh && !fanState) {
    setFan(true);
  } else if (gTemp < tempThresholdLow && fanState) {
    setFan(false);
  }
}

// ── Blue light: ambient light with hysteresis (threshold ± 50) ──
// The blue LED is shared with the breathing effect, PIR and manual control:
// skip auto control while any of them owns the LED, to avoid fighting.
void controlLight() {
  if (!lightAuto || breatheLedState || gPir) return;
  if (gLight < (lightThreshold - 50) && !blueLedState) {
    setBlueLed(true);
  } else if (gLight > (lightThreshold + 50) && blueLedState) {
    setBlueLed(false);
  }
}

// ── RGB: plant growth light when dark (activity 9), off when bright ──
void controlRgb() {
  if (!rgbAuto) return;
  static bool sRgbWasDark = false;
  bool isDark = gLight < (lightThreshold - 50);
  bool isBright = gLight > (lightThreshold + 50);
  if (isDark && !sRgbWasDark) {
    setRgbPlant();
    sRgbWasDark = true;
  } else if (isBright && sRgbWasDark) {
    setRgb(0, 0, 0);
    sRgbWasDark = false;
  }
}

// ── Breathe LED: smooth PWM ramp while breatheLedState is on ──
void controlBreathe() {
  if (!breatheLedState) {
    if (breatheBrightness != 0) {
      breatheBrightness = 0;
      setLedPwm(0);
    }
    return;
  }

  unsigned long now = millis();
  if (now - lastBreatheUpdate < BREATHE_INTERVAL) return;
  lastBreatheUpdate = now;

  int16_t next = (int16_t)breatheBrightness + (int16_t)breatheDirection * 5;
  if (next >= 255) {
    next = 255;
    breatheDirection = -1;
  } else if (next <= 0) {
    next = 0;
    breatheDirection = 1;
  }
  breatheBrightness = (uint8_t)next;
  setLedPwm(breatheBrightness);
}

// ── Touch: manual blue LED toggle, one toggle per press (activity 4) ──
void controlTouch() {
  static bool sPrevTouch = false;
  if (gTouch && !sPrevTouch) {
    unsigned long now = millis();
    if (now - lastTouch >= TOUCH_DEBOUNCE) {
      if (lightAuto) {  // manual action disables auto to avoid fighting
        lightAuto = false;
        saveSettings();
      }
      setBlueLed(!blueLedState);
      lastTouch = now;
      LOG_INFO("Touch: LED toggle");
    }
  }
  sPrevTouch = gTouch;
}

// ── Alerts: low-water continuous buzzer (priority) + PIR night light ──
void controlAlerts() {
  if (gWater < (int)waterThresholdLow) {
    digitalWrite(BUZZER_PIN, HIGH);
    if (!sWaterAlarm) {
      sWaterAlarm = true;
      LOG_ERROR_CODE(302, "Low water alarm");
    }
    return;
  }
  if (sWaterAlarm) {
    sWaterAlarm = false;
    digitalWrite(BUZZER_PIN, LOW);
  }

  // PIR (activity 5): light on while motion is detected, off when it stops
  if (gPir && !sMotionAlarm) {
    sMotionAlarm = true;
    setBlueLed(true);
    LOG_ERROR_CODE(301, "Motion detected");
  } else if (!gPir && sMotionAlarm) {
    sMotionAlarm = false;
    setBlueLed(false);
  }
}
