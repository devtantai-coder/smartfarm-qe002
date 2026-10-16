#include "globals.h"
#include "logger.h"
#include "config.h"
#include "actuators.h"
#include "settings.h"
#include "control.h"

// ── Alert state (edge-triggered logs, no serial spam) ──
static bool sWaterAlarm = false;
static bool sMotionAlarm = false;
static unsigned long sMotionStart = 0;

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
void controlLight() {
  if (!lightAuto) return;
  if (gLight < (lightThreshold - 50) && !blueLedState) {
    setBlueLed(true);
  } else if (gLight > (lightThreshold + 50) && blueLedState) {
    setBlueLed(false);
  }
}

// ── RGB: red when dark, off when bright (with hysteresis) ──
void controlRgb() {
  if (!rgbAuto) return;
  static bool sRgbWasDark = false;
  bool isDark = gLight < (lightThreshold - 50);
  bool isBright = gLight > (lightThreshold + 50);
  if (isDark && !sRgbWasDark) {
    setRgb(255, 0, 0);
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
      setBreathePwm(0);
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
  setBreathePwm(breatheBrightness);
}

// ── Touch: manual pump toggle, one toggle per press (rising edge) ──
void controlTouch() {
  static bool sPrevTouch = false;
  if (gTouch && !sPrevTouch) {
    unsigned long now = millis();
    if (now - lastTouch >= TOUCH_DEBOUNCE) {
      if (pumpAuto) {  // manual action disables auto to avoid fighting
        pumpAuto = false;
        saveSettings();
      }
      setRelay(!relayState);
      lastTouch = now;
      LOG_INFO("Touch: manual pump toggle");
    }
  }
  sPrevTouch = gTouch;
}

// ── Alerts: low-water alarm (priority, continuous) + PIR beep ──
void controlAlerts() {
  unsigned long now = millis();

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

  if (gPir) {
    setBlueLed(true);
    if (!sMotionAlarm) {
      sMotionAlarm = true;
      sMotionStart = now;
      LOG_ERROR_CODE(301, "Motion detected");
    }
    bool beepOn = ((now - sMotionStart) % BUZZER_INTERVAL) < (BUZZER_INTERVAL / 2);
    digitalWrite(BUZZER_PIN, beepOn ? HIGH : LOW);
  } else if (sMotionAlarm) {
    sMotionAlarm = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}
