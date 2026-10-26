#include "softstart.h"
#include "globals.h"
#include "actuators.h"

static bool sActive = false;
static unsigned long sStart = 0;
static bool sTarget = false;

void softStartInit() { sActive = false; }
void softStartPump(bool on) {
  sTarget = on;
  if (on) { sActive = true; sStart = millis(); }
  else { sActive = false; setRelay(false); }
}
void softStartUpdate() {
  if (!sActive) return;
  if (millis() - sStart >= 500) {
    setRelay(sTarget);
    sActive = false;
  }
}
bool softStartIsActive() { return sActive; }
