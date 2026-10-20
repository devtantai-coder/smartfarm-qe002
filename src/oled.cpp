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

// ── Đẩy framebuffer qua I2C theo PAGE thay đổi ──
// display.display() của thư viện luôn gửi đủ 1024 byte (~23 ms ở 400 kHz).
// Ở đây chỉ gửi các page 8-hàng thực sự đổi (mỗi page 128 byte ≈ 3 ms):
// thường chỉ 1-4 dòng số liệu đổi nên mỗi lần cập nhật rẻ hơn nhiều.
static uint8_t sPrevFrame[SCREEN_WIDTH * SCREEN_HEIGHT / 8];
static bool sPrevFrameValid = false;

// 127 byte dữ liệu + 1 byte điều khiển = 128 = buffer I2C mặc định của Wire.
static const size_t OLED_CHUNK = 127;

static void oledWriteData(const uint8_t* data, size_t n) {
  size_t sent = 0;
  while (sent < n) {
    size_t chunk = n - sent;
    if (chunk > OLED_CHUNK) chunk = OLED_CHUNK;
    Wire.beginTransmission(OLED_I2C_ADDR);
    Wire.write((uint8_t)0x40);   // control: D/C# = 1 (luồng dữ liệu)
    Wire.write(data + sent, chunk);
    Wire.endTransmission();
    sent += chunk;
  }
}

static void oledWritePages(const uint8_t* frame, uint8_t p0, uint8_t p1) {
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write((uint8_t)0x00);                 // control: toàn lệnh
  Wire.write((uint8_t)0x21);                 // set column address
  Wire.write((uint8_t)0);                    // từ cột 0
  Wire.write((uint8_t)(SCREEN_WIDTH - 1));   // đến cột cuối
  Wire.write((uint8_t)0x22);                 // set page address
  Wire.write(p0);
  Wire.write(p1);
  Wire.endTransmission();
  oledWriteData(frame + (size_t)p0 * SCREEN_WIDTH, (size_t)(p1 - p0 + 1) * SCREEN_WIDTH);
}

void oledForceRedraw() { sPrevFrameValid = false; }

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
  display.print(sys.relayState ? F("ON") : F("OFF"));

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
  } else if (sys.windmillOn) {
    display.print(F("Coi xay: ON"));
  } else if (feedRunning()) {
    display.print(F("Cho an..."));
  } else {
    display.print(F("San sang"));
  }

  const uint8_t pages = SCREEN_HEIGHT / 8;
  const uint8_t* frame = display.getBuffer();
  uint8_t p0 = 0xFF, p1 = 0;
  if (sPrevFrameValid) {
    for (uint8_t p = 0; p < pages; p++) {
      if (memcmp(frame + (size_t)p * SCREEN_WIDTH, sPrevFrame + (size_t)p * SCREEN_WIDTH,
                 SCREEN_WIDTH) != 0) {
        if (p0 == 0xFF) p0 = p;
        p1 = p;
      }
    }
    if (p0 == 0xFF) return;                    // giống hệt: không chạm I2C
  } else {
    p0 = 0;
    p1 = pages - 1;
  }
  memcpy(sPrevFrame, frame, sizeof(sPrevFrame));
  sPrevFrameValid = true;
  oledWritePages(frame, p0, p1);
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: coi như màn hình vừa bật (khung trước không hợp lệ).
void testResetOledStatics() { sPrevFrameValid = false; }
#endif
