#include "../softstart.h"
#include "../globals.h"
#include "../actuators.h"
#include "../config.h"

static bool sActive = false;
static unsigned long sStart = 0;

void softStartInit() { sActive = false; sStart = 0; }

void softStartCancel() { sActive = false; }

void softStartPump(bool on) {
  if (!on) {
    sActive = false;
    setRelay(false);
    return;
  }
  if (sys.relayState) {   // đã chạy: không trễ lần nữa
    sActive = false;
    return;
  }
  sActive = true;
  sStart = millis();
}

void softStartUpdate() {
  if (!sActive) return;

  // Hủy hẹn giờ nếu điều kiện an toàn/ý định đã đổi trong lúc chờ.
  if (!sys.pumpAuto || sys.gWater < (int)sys.waterThresholdLow ||
      sys.gSoil > (int)sys.soilThresholdHigh) {
    sActive = false;
    return;
  }

  if (millis() - sStart >= SOFTSTART_MS) {
    sActive = false;
    setRelay(true);
  }
}

bool softStartIsActive() { return sActive; }
