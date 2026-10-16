#include "globals.h"
#include "logger.h"
#include "actuators.h"
#include "control.h"
#include "demo.h"

// Non-blocking showcase of the kit activities (2-12). While the demo runs,
// main loop() pauses the normal automation so nothing fights over the pins.
enum DemoAction {
  DA_NONE, DA_LED_ON, DA_LED_OFF, DA_BREATHE_ON, DA_BREATHE_OFF,
  DA_PUMP_ON, DA_PUMP_OFF, DA_BEEP, DA_PLANT_ON, DA_RGB_OFF,
  DA_WINDMILL, DA_SERVO0, DA_FEED
};

struct DemoStep {
  DemoAction in;
  DemoAction out;
  uint16_t ms;
  const char* name;
};

static const DemoStep STEPS[] = {
  { DA_LED_ON,     DA_LED_OFF,     2000, "HD2: Thap sang" },
  { DA_BREATHE_ON, DA_BREATHE_OFF, 4000, "HD3: Den tho PWM" },
  { DA_LED_ON,     DA_NONE,        1200, "HD4: Cham LED (bat)" },
  { DA_LED_OFF,    DA_NONE,        1000, "HD4: Cham LED (tat)" },
  { DA_LED_ON,     DA_NONE,        2500, "HD5: PIR co nguoi" },
  { DA_LED_OFF,    DA_NONE,        1000, "HD5: Nguoi roi di" },
  { DA_WINDMILL,   DA_SERVO0,      4000, "HD6: Coi xay gio quay" },
  { DA_PUMP_ON,    DA_PUMP_OFF,    2500, "HD7: Tuoi tu dong" },
  { DA_BEEP,       DA_NONE,        1000, "HD8: Bao nuoc can" },
  { DA_PLANT_ON,   DA_RGB_OFF,     3000, "HD9: Den tang truong" },
  { DA_FEED,       DA_NONE,        3000, "HD10: Cho an tu dong" },
  { DA_SERVO0,     DA_NONE,        800,  "HD10: Dong cua" },
  { DA_NONE,       DA_NONE,        2500, "HD11: Xem OLED" },
  { DA_LED_ON,     DA_NONE,        1200, "HD12: Dieu khien" },
  { DA_LED_OFF,    DA_NONE,        1000, "HD12: Tat tu xa" },
};

static const uint8_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);

static bool sRunning = false;
static uint8_t sStep = 0;
static unsigned long sStepStart = 0;
static unsigned long sSweepUpdate = 0;

bool demoRunning() { return sRunning; }

const char* demoStepName() {
  return sRunning ? STEPS[sStep].name : "";
}

static void demoAction(DemoAction a) {
  switch (a) {
    case DA_LED_ON:      setBlueLed(true); break;
    case DA_LED_OFF:     setBlueLed(false); break;
    case DA_BREATHE_ON:  setBreatheLed(true); break;
    case DA_BREATHE_OFF: setBreatheLed(false); break;
    case DA_PUMP_ON:
      if (gWater >= (int)waterThresholdLow) setRelay(true);
      else LOG_WARN("Demo: low water, skip pump");
      break;
    case DA_PUMP_OFF:    setRelay(false); break;
    case DA_BEEP:        setBuzzer(true); break;
    case DA_PLANT_ON:    setRgbPlant(); break;
    case DA_RGB_OFF:     setRgb(0, 0, 0); break;
    case DA_WINDMILL:    break;  // swept continuously in updateDemo()
    case DA_SERVO0:      servo.write(WINDMILL_MIN_ANGLE); stopFeed(); break;
    case DA_FEED:        startFeed(); break;
    default: break;
  }
}

static void demoLogStep() {
  Serial.print(F("[DEMO] "));
  Serial.println(STEPS[sStep].name);
}

void startDemo() {
  if (sRunning) return;
  setWindmill(false);
  stopFeed();
  sRunning = true;
  sStep = 0;
  sStepStart = millis();
  LOG_INFO("===== DEMO START =====");
  demoLogStep();
  demoAction(STEPS[sStep].in);
}

void updateDemo() {
  if (!sRunning) return;
  unsigned long now = millis();
  unsigned long elapsed = now - sStepStart;

  // HD6: sweep the servo like rotating windmill blades
  if (STEPS[sStep].in == DA_WINDMILL && now - sSweepUpdate >= WINDMILL_INTERVAL) {
    sSweepUpdate = now;
    int span = WINDMILL_MAX_ANGLE - WINDMILL_MIN_ANGLE;
    int phase = (int)(elapsed % 2000);
    int angle = WINDMILL_MIN_ANGLE + ((phase < 1000) ? (phase * span) / 1000
                                                     : ((2000 - phase) * span) / 1000);
    servo.write(angle);
  }

  if (elapsed < STEPS[sStep].ms) return;

  demoAction(STEPS[sStep].out);
  if (STEPS[sStep].in == DA_BEEP) setBuzzer(false);

  sStep++;
  if (sStep >= STEP_COUNT) {
    sRunning = false;
    setBlueLed(false);
    setBreatheLed(false);
    setRelay(false);
    setBuzzer(false);
    setRgb(0, 0, 0);
    servo.write(WINDMILL_MIN_ANGLE);
    stopFeed();
    if (breatheAuto) setBreatheLed(true);   // restore opt-in breathing (activity 3)
    LOG_INFO("===== DEMO DONE =====");
    return;
  }

  sStepStart = now;
  demoLogStep();
  demoAction(STEPS[sStep].in);
}
