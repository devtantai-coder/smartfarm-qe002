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
  // Safety first, even in manual mode (activity 8): running the pump dry
  // would damage it, so low water always forces the pump off.
  if (gWater < (int)waterThresholdLow) {
    if (relayState) {
      setRelay(false);
      LOG_ERROR_CODE(201, "Low water, pump OFF");
    }
    return;
  }

  if (!pumpAuto) return;

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
// Stateless: compares against the actual RGB color so manual colors are
// never left on while auto mode is re-enabled.
void controlRgb() {
  if (!rgbAuto) return;
  bool isDark = gLight < (lightThreshold - 50);
  bool isBright = gLight > (lightThreshold + 50);
  bool plantOn = (rgbR == 255 && rgbG == 0 && rgbB == 255);
  bool rgbOff = (rgbR == 0 && rgbG == 0 && rgbB == 0);
  if (isDark && !plantOn) {
    setRgbPlant();
  } else if (isBright && !rgbOff) {
    setRgb(0, 0, 0);
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

// ── Windmill (activity 6): sweep the servo back and forth like rotating blades ──
static int sWindmillAngle = 0;
static int8_t sWindmillDir = 1;
static unsigned long sWindmillUpdate = 0;

void setWindmill(bool on) {
  if (windmillOn == on) return;
  windmillOn = on;
  if (on) {
    stopFeed();             // shared servo
  } else {
    servo.write(0);
  }
  LOG_INFO(on ? "Windmill: ON" : "Windmill: OFF");
}

void controlWindmill() {
  if (!windmillOn) return;
  unsigned long now = millis();
  if (now - sWindmillUpdate < WINDMILL_INTERVAL) return;
  sWindmillUpdate = now;
  sWindmillAngle += sWindmillDir * 6;
  if (sWindmillAngle >= 180) { sWindmillAngle = 180; sWindmillDir = -1; }
  if (sWindmillAngle <= 0)   { sWindmillAngle = 0;   sWindmillDir = 1;  }
  servo.write(sWindmillAngle);
}

// ── Feed gate (activity 10): servo opens the bowl for FEED_DURATION then closes ──
static bool sFeedActive = false;
static unsigned long sFeedStart = 0;

void startFeed() {
  setWindmill(false);       // shared servo
  sFeedActive = true;
  sFeedStart = millis();
  servo.write(90);          // gate open
  LOG_INFO("Feed: gate open");
}

void stopFeed() {
  sFeedActive = false;
}

void controlFeed() {
  if (!sFeedActive) return;
  if (millis() - sFeedStart >= FEED_DURATION) {
    sFeedActive = false;
    servo.write(0);         // gate closed
    LOG_INFO("Feed: gate closed");
  }
}

bool feedRunning() { return sFeedActive; }
