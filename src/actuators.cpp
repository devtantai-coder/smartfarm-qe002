#include "globals.h"
#include "logger.h"
#include "actuators.h"

// Mọi hàm setter đều idempotent: không ghi lại / không spam log khi trạng thái
// không đổi. Nhờ vậy controlAlerts()/controlLight() vẫn rẻ dù
// được gọi mỗi vòng lặp.

void setRelay(bool on) {
  if (sys.relayState == on) return;
  sys.relayState = on;
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  LOG_INFO(on ? "Pump: ON" : "Pump: OFF");
}

// Hàm phụ buzzer: buzzer active chỉ cần mức digital, buzzer passive
// phải được điều khiển bằng xung vuông. Idempotent như các setter khác.
static bool sBuzzerOn = false;

void setBuzzer(bool on) {
  if (sBuzzerOn == on) return;
  sBuzzerOn = on;
#if BUZZER_PASSIVE
  if (on) tone(BUZZER_PIN, BUZZER_TONE_HZ);
  else    noTone(BUZZER_PIN);
#else
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
#endif
  if (on) LOG_INFO("Buzzer: ON");
}

void setFan(bool on) {
  if (sys.fanState == on) return;
  sys.fanState = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Fan: ON" : "Fan: OFF");
}

// Blue LED (chân 27) được điều khiển bằng PWM để cả chế độ bật/tắt liên tục
// và hiệu ứng thở dùng chung phần cứng. Hai chế độ loại trừ
// lẫn nhau (sys.blueLedState vs sys.breatheLedState); lệnh sáng liên tục thắng.
void setBlueLed(bool on) {
  if (on) {
    if (sys.blueLedState && !sys.breatheLedState) return;
    sys.blueLedState = true;
    sys.breatheLedState = false;    // đèn sáng liên tục thắng chế độ thở
    sys.breatheBrightness = 0;
    setLedPwm(255);
  } else {
    if (!sys.blueLedState && !sys.breatheLedState) return;
    sys.blueLedState = false;
    sys.breatheLedState = false;
    sys.breatheBrightness = 0;
    setLedPwm(0);
  }
  LOG_INFO(on ? "Blue LED: ON" : "Blue LED: OFF");
}

// Nơi duy nhất giao tiếp với ngoại vi PWM (core 2.x dùng channel,
// core 3.x dùng chân).
void setLedPwm(uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BLUE_LED_PIN, value);
#else
  ledcWrite(LED_PWM_CHANNEL, value);
#endif
}

void setBreatheLed(bool on) {
  if (sys.breatheLedState == on) return;
  sys.breatheLedState = on;
  sys.breatheBrightness = 0;
  sys.breatheDirection = 1;
  if (on) {
    sys.blueLedState = false;       // chế độ thở tiếp quản blue LED
  } else {
    setLedPwm(0);
  }
  LOG_INFO(on ? "Breathe: ON" : "Breathe: OFF");
}

// true = hoa văn đèn tăng trưởng đang hiển thị. sys.rgbR/G/B không phân biệt
// được hoa văn với màu đơn sắc cùng giá trị (255,0,255), nên cần cờ riêng để
// setRgb() idempotent đúng: bỏ qua strip.show() (một giao dịch RMT ngắt
// interrupts) khi màu không đổi thật sự.
static bool sPlantPattern = false;

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (!sPlantPattern && sys.rgbR == r && sys.rgbG == g && sys.rgbB == b) return;
  sPlantPattern = false;
  sys.rgbR = r; sys.rgbG = g; sys.rgbB = b;
  for (int i = 0; i < RGB_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB set");
}

// Đèn tăng trưởng cho cây (bài 9): LED 1 & 3 đỏ, LED 2 & 4 xanh dương.
void setRgbPlant() {
  sPlantPattern = true;
  sys.rgbR = 255; sys.rgbG = 0; sys.rgbB = 255;
  for (int i = 0; i < RGB_COUNT; i++) {
    bool red = (i % 2 == 0);
    strip.setPixelColor(i, red ? strip.Color(255, 0, 0) : strip.Color(0, 0, 255));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB plant light");
}

void setRgbAuto(bool on)  { sys.rgbAuto = on; }
void setPumpAuto(bool on) { sys.pumpAuto = on; }
void setFanAuto(bool on)  { sys.fanAuto = on; }
void setLightAuto(bool on){ sys.lightAuto = on; }
void setBreatheAuto(bool on) { sys.breatheAuto = on; }

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test trên máy tính: đưa biến static về trạng thái vừa cấp điện
// để các test không phụ thuộc vào thứ tự chạy.
void testResetActuatorStatics() { sBuzzerOn = false; sPlantPattern = false; }
#endif
