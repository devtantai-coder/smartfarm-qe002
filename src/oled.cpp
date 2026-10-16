#include "globals.h"
#include "logger.h"
#include "sensors.h"
#include "control.h"
#include "demo.h"
#include "oled.h"

// Nhãn chỉ dùng ASCII: font mặc định SSD1306 không có dấu tiếng Việt
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

  // Thanh tiêu đề
  display.setCursor(0, 0);
  display.print(F("Smart Farm"));

  // Dòng 1: trạng thái thời tiết (hoạt động 11)
  display.setCursor(0, 8);
  display.print(F("TT: "));
  display.print(weatherText());

  // Dòng 2: Nhiệt độ + Độ ẩm
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(t, 1);
  display.print(F("C H:"));
  display.print(h, 0);
  display.print(F("%"));

  // Dòng 3: Ánh sáng
  display.setCursor(0, 24);
  display.print(F("Light: "));
  display.print(light);

  // Dòng 4: Đất
  display.setCursor(0, 32);
  display.print(F("Soil: "));
  display.print(soil);

  // Dòng 5: Nước + trạng thái máy bơm
  display.setCursor(0, 40);
  display.print(F("H2O:"));
  display.print(water);
  display.print(F(" P:"));
  display.print(relayState ? F("ON") : F("OFF"));

  // Dòng 6: IP WiFi
  display.setCursor(0, 48);
  if (WiFi.status() == WL_CONNECTED) {
    display.print(F("IP:"));
    display.print(WiFi.localIP());
  } else {
    display.print(F("WiFi: ---"));
  }

  // Dòng 7: trạng thái demo / cơ cấu
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
