#include "globals.h"
#include "logger.h"
#include "oled.h"

void updateOLED(float t, float h, int light, int soil, int water) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Title bar
  display.setCursor(0, 0);
  display.print(F("Smart Farm"));

  // Row 1: Temp + Humidity
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(t, 1);
  display.print(F("C H:"));
  display.print(h, 0);
  display.print(F("%"));

  // Row 2: Light
  display.setCursor(0, 32);
  display.print(F("Light: "));
  display.print(light);

  // Row 3: Soil
  display.setCursor(0, 48);
  display.print(F("Soil: "));
  display.print(soil);

  // Row 4: Water + Pump state (y=56: 8px text fits exactly with 64px screen)
  display.setCursor(0, 56);
  display.print(F("H2O:"));
  display.print(water);
  display.print(F(" P:"));
  display.print(relayState ? F("ON") : F("OFF"));

  display.display();
}
