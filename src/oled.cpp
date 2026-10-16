#include "globals.h"
#include "logger.h"
#include "sensors.h"
#include "control.h"
#include "demo.h"
#include "oled.h"

// ASCII-only labels: the SSD1306 default font has no Vietnamese diacritics
static const char* weatherText() {
  switch (weatherStatus()) {
    case WEATHER_SUNNY:  return "Nang";
    case WEATHER_CLOUDY: return "Am u";
    case WEATHER_RAIN:   return "Mua";
    default:             return "B.thuong";
  }
}

void updateOLED(float t, float h, int light, int soil, int water) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Title bar
  display.setCursor(0, 0);
  display.print(F("Smart Farm"));

  // Row 1: weather status (activity 11)
  display.setCursor(0, 8);
  display.print(F("TT: "));
  display.print(weatherText());

  // Row 2: Temp + Humidity
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(t, 1);
  display.print(F("C H:"));
  display.print(h, 0);
  display.print(F("%"));

  // Row 3: Light
  display.setCursor(0, 24);
  display.print(F("Light: "));
  display.print(light);

  // Row 4: Soil
  display.setCursor(0, 32);
  display.print(F("Soil: "));
  display.print(soil);

  // Row 5: Water + Pump state
  display.setCursor(0, 40);
  display.print(F("H2O:"));
  display.print(water);
  display.print(F(" P:"));
  display.print(relayState ? F("ON") : F("OFF"));

  // Row 6: WiFi IP
  display.setCursor(0, 48);
  if (WiFi.status() == WL_CONNECTED) {
    display.print(F("IP:"));
    display.print(WiFi.localIP());
  } else {
    display.print(F("WiFi: ---"));
  }

  // Row 7: demo / mechanism status
  display.setCursor(0, 56);
  if (demoRunning()) {
    display.print(demoStepName());
  } else if (windmillOn) {
    display.print(F("Coi xay: ON"));
  } else if (feedRunning()) {
    display.print(F("Cho an..."));
  } else {
    display.print(F("San sang"));
  }

  display.display();
}
