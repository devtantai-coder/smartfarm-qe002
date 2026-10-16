#include "globals.h"
#include "logger.h"
#include "config.h"
#include "actuators.h"
#include "control.h"

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

// ── Blue light: ambient light with hysteresis ──
void controlLight() {
  if (!lightAuto) return;
  // Hysteresis: turn on below (threshold - 50), turn off above (threshold + 50)
  // Prevents rapid toggling when light fluctuates around threshold
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
  // Hysteresis: dark < (threshold - 50), bright > (threshold + 50)
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

// ── Breathe LED: smooth PWM ramp ──
void controlBreathe() {
  if (!breatheAuto) return;
  unsigned long now = millis();
  if (now - lastBreatheUpdate < BREATHE_INTERVAL) return;

  breatheBrightness += breatheDirection * 5;
  if (breatheBrightness >= 255) {
    breatheBrightness = 255;
    breatheDirection = -1;
  } else if (breatheBrightness <= 0) {
    breatheBrightness = 0;
    breatheDirection = 1;
  }
  ledcWrite(BREATHE_LED_CHANNEL, breatheBrightness);
  lastBreatheUpdate = now;
}

// ── Touch: debounced relay toggle ──
void controlTouch() {
  if (!gTouch) return;
  unsigned long now = millis();
  if (now - lastTouch < TOUCH_DEBOUNCE) return;
  setRelay(!relayState);
  lastTouch = now;
}

// ── Alerts: PIR + low water ──
void controlAlerts() {
  unsigned long now = millis();

  if (gPir) {
    setBlueLed(true);
    if (now - lastBuzzer > BUZZER_INTERVAL) {
      digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
      lastBuzzer = now;
      LOG_ERROR_CODE(301, "Motion detected");
    }
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  if (gWater < (int)waterThresholdLow) {
    digitalWrite(BUZZER_PIN, HIGH);
    LOG_ERROR_CODE(302, "Low water alarm");
  }
}
