#include "globals.h"
#include "logger.h"
#include "config.h"
#include "actuators.h"
#include "settings.h"
#include "control.h"

// ── Trạng thái cảnh báo (log edge-triggered, không spam serial) ──
static bool sWaterAlarm = false;
static bool sMotionAlarm = false;
static bool sPirLitLed = false;   // true = cảnh báo PIR là thứ đã bật LED
static bool sPrevTouch = false;   // trạng thái chạm lần trước (chống dội)

// ── Bơm: độ ẩm đất + an toàn nước ──
void controlPump() {
  // An toàn là trên hết, kể cả chế độ thủ công (bài 8): chạy bơm khô
  // sẽ làm hỏng bơm, nên nước thấp luôn buộc tắt bơm.
  if (gWater < (int)waterThresholdLow) {
    if (relayState) {
      setRelay(false);
      LOG_ERROR_CODE(201, "Low water, pump OFF");
    }
    return;
  }

  if (!pumpAuto) return;

  // Chống ngưỡng bị đảo (sẽ làm relay đóng ngắt liên tục)
  if (soilThresholdHigh <= soilThresholdLow) return;

  // Hysteresis: bật khi dưới ngưỡng thấp, tắt khi trên ngưỡng cao
  if (gSoil < (int)soilThresholdLow && !relayState) {
    setRelay(true);
    LOG_INFO("Soil dry, pump ON [202]");
  } else if (gSoil > (int)soilThresholdHigh && relayState) {
    setRelay(false);
    LOG_INFO("Soil wet, pump OFF [203]");
  }
}

// ── Quạt: hysteresis theo nhiệt độ ──
void controlFan() {
  if (!fanAuto) return;

  // Chống ngưỡng bị đảo (sẽ làm relay đóng ngắt liên tục)
  if (tempThresholdHigh <= tempThresholdLow) return;

  if (gTemp > tempThresholdHigh && !fanState) {
    setFan(true);
  } else if (gTemp < tempThresholdLow && fanState) {
    setFan(false);
  }
}

// ── Đèn xanh dương: ánh sáng môi trường với hysteresis (ngưỡng ± 50) ──
// Blue LED dùng chung với hiệu ứng thở, PIR và điều khiển thủ công:
// bỏ qua điều khiển tự động khi một trong số đó đang giữ LED, để tránh tranh nhau.
void controlLight() {
  if (!lightAuto || breatheLedState || gPir) return;
  if (gLight < (lightThreshold - 50) && !blueLedState) {
    setBlueLed(true);
  } else if (gLight > (lightThreshold + 50) && blueLedState) {
    setBlueLed(false);
  }
}

// ── RGB: đèn tăng trưởng cây khi trời tối (bài 9), tắt khi trời sáng ──
// Không lưu trạng thái: so sánh với màu RGB thực tế nên màu thủ công
// không bao giờ bị bỏ quên khi bật lại chế độ tự động.
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

// ── LED thở: tăng giảm PWM mượt trong khi breatheLedState bật ──
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

// ── Chạm: bật/tắt blue LED thủ công, mỗi lần nhấn đổi một lần (bài 4) ──
void controlTouch() {
  if (gTouch && !sPrevTouch) {
    unsigned long now = millis();
    if (now - lastTouch >= TOUCH_DEBOUNCE) {
      if (lightAuto) {  // thao tác thủ công tắt auto để tránh tranh nhau
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

// ── Cảnh báo: buzzer liên tục khi nước thấp (ưu tiên) + đèn đêm PIR ──
void controlAlerts() {
  if (gWater < (int)waterThresholdLow) {
    setBuzzer(true);
    if (!sWaterAlarm) {
      sWaterAlarm = true;
      LOG_ERROR_CODE(302, "Low water alarm");
    }
    return;
  }
  if (sWaterAlarm) {
    sWaterAlarm = false;
    setBuzzer(false);
  }

  // PIR (bài 5): bật đèn khi phát hiện chuyển động, tắt khi hết chuyển động.
  // Cảnh báo ghi nhớ nó có phải thứ đã bật LED không, nên đèn đang sáng
  // (thủ công hay tự động) không bị người đi qua tắt mất. Khi hết
  // chuyển động, chế độ thở đã bật trước đó (bài 3) được khôi phục.
  if (gPir && !sMotionAlarm) {
    sMotionAlarm = true;
    sPirLitLed = !blueLedState;
    if (sPirLitLed) setBlueLed(true);
    LOG_ERROR_CODE(301, "Motion detected");
  } else if (!gPir && sMotionAlarm) {
    sMotionAlarm = false;
    if (sPirLitLed) {
      sPirLitLed = false;
      if (breatheAuto) setBreatheLed(true);
      else setBlueLed(false);
    }
  }
}

// ── Cối xay gió (bài 6): quét servo qua lại như cánh quạt đang quay ──
static int sWindmillAngle = 0;
static int8_t sWindmillDir = 1;
static unsigned long sWindmillUpdate = 0;

void setWindmill(bool on) {
  if (windmillOn == on) return;
  windmillOn = on;
  if (on) {
    stopFeed();             // servo dùng chung
  } else {
    servo.write(WINDMILL_MIN_ANGLE);
  }
  LOG_INFO(on ? "Windmill: ON" : "Windmill: OFF");
}

void controlWindmill() {
  if (!windmillOn) return;
  unsigned long now = millis();
  if (now - sWindmillUpdate < WINDMILL_INTERVAL) return;
  sWindmillUpdate = now;
  sWindmillAngle += sWindmillDir * 6;
  if (sWindmillAngle >= WINDMILL_MAX_ANGLE) { sWindmillAngle = WINDMILL_MAX_ANGLE; sWindmillDir = -1; }
  if (sWindmillAngle <= WINDMILL_MIN_ANGLE) { sWindmillAngle = WINDMILL_MIN_ANGLE; sWindmillDir = 1;  }
  servo.write(sWindmillAngle);
}

// ── Cửa cho ăn (bài 10): servo mở bát trong FEED_DURATION rồi đóng lại ──
static bool sFeedActive = false;
static unsigned long sFeedStart = 0;

void startFeed() {
  setWindmill(false);       // servo dùng chung
  sFeedActive = true;
  sFeedStart = millis();
  servo.write(FEED_OPEN_ANGLE);   // cửa mở
  LOG_INFO("Feed: gate open");
}

void stopFeed() {
  sFeedActive = false;
}

void controlFeed() {
  if (!sFeedActive) return;
  if (millis() - sFeedStart >= FEED_DURATION) {
    sFeedActive = false;
    servo.write(FEED_CLOSE_ANGLE);  // cửa đóng
    LOG_INFO("Feed: gate closed");
  }
}

bool feedRunning() { return sFeedActive; }

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: đưa các biến static về trạng thái vừa cấp điện
// để mỗi test độc lập với thứ tự chạy.
void testResetControlStatics() {
  sWaterAlarm = false;
  sMotionAlarm = false;
  sPirLitLed = false;
  sPrevTouch = false;
  sWindmillAngle = 0;
  sWindmillDir = 1;
  sWindmillUpdate = 0;
  sFeedActive = false;
  sFeedStart = 0;
}
#endif
