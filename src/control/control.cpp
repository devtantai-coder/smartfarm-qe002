#include "../globals.h"
#include "../logger.h"
#include "../config.h"
#include "../actuators.h"
#include "../settings.h"
#include "../control.h"
#include "../hal.h"
#include "../softstart.h"
#include "../connectivity/notify.h"
#include "../pid.h"

// ── Hằng số điều khiển (đặt tên rõ ràng thay vì "số ma thuật") ──
static constexpr int LIGHT_HYSTERESIS = 50;   // biên dead-band quanh ngưỡng ánh sáng
static constexpr int BREATHE_STEP    = 5;     // bước đổi độ sáng mỗi nhịp thở
static constexpr int BREATHE_MAX     = 255;   // độ sáng tối đa (8 bit PWM)
static constexpr int WINDMILL_STEP   = 6;     // bước quét servo mỗi nhịp (độ)

// ── Trạng thái cảnh báo (log edge-triggered, không spam serial) ──
static bool sWaterAlarm = false;
static bool sMotionAlarm = false;
static bool sManualAlarm = false;
static bool sPirLitLed = false;   // true = cảnh báo PIR là thứ đã bật LED
static bool sPirPrevBreathe = false;  // LED thở đang chạy trước khi PIR chiếm đèn
static bool sPrevTouch = false;   // trạng thái chạm lần trước (chống dội)
static bool sFanFailSafeWarned = false;  // đã báo "quạt fail-safe" cho đợt hỏng DHT này

// ── Bơm: độ ẩm đất + an toàn nước ──
void controlPump() {
  // An toàn là trên hết, kể cả chế độ thủ công (bài 8): chạy bơm khô
  // sẽ làm hỏng bơm, nên nước thấp luôn buộc tắt bơm.
  const int waterLow = (int)sys.waterThresholdLow;
  if (sys.gWater < waterLow) {
    softStartPump(false);   // hủy cả lần bật đang chờ
    if (sys.relayState) {
      LOG_ERROR_CODE(201, "Low water, pump OFF");
    }
    return;
  }

  // Lưu ý: KHÔNG gate theo sys.sensorHealthy ở đây. Cờ đó do driver DHT11
  // quản lý (đếm lần đọc trả NaN), trong khi bơm chỉ phụ thuộc cảm biến đất +
  // cảm biến nước (điện trở, không liên quan DHT). Trước đây DHT chết là tưới
  // tự động im lặng dừng dù đất vẫn khô — hỏng một cảm biến làm hỏng cả hệ.
  if (!sys.pumpAuto) return;

  const float soilLow = sys.soilThresholdLow;
  const float soilHigh = sys.soilThresholdHigh;
  // Chống ngưỡng bị đảo (sẽ làm relay đóng ngắt liên tục)
  if (soilHigh <= soilLow) return;

  // Hysteresis: bật khi dưới ngưỡng thấp, tắt khi trên ngưỡng cao.
  // Bật qua soft-start để giảm sốc dòng cho bơm.
  if (sys.gSoil < (int)soilLow && !sys.relayState && !softStartIsActive()) {
    softStartPump(true);
    LOG_INFO("Soil dry, pump starting [202]");
  } else if (sys.gSoil > (int)soilHigh && sys.relayState) {
    softStartPump(false);
    LOG_INFO("Soil wet, pump OFF [203]");
  }
}

// ── Quạt: hysteresis theo nhiệt độ ──
void controlFan() {
  if (!sys.fanAuto) return;

  const int waterLow = (int)sys.waterThresholdLow;
  // An toàn nước: nếu nước cạn, tắt quạt (nếu quạt liên quan đến nước)
  if (sys.gWater < waterLow) {
    if (sys.fanState) {
      setFan(false);
      LOG_WARN("Low water, fan OFF");
    }
    return;
  }

  // Quạt là thiết bị duy nhất quyết định theo NHIỆT ĐỘ — tức theo DHT. Khi driver
  // báo hỏng, sys.gTemp đóng băng ở trung bình tốt cuối cùng (bộ lọc không còn
  // viết bịa mẫu nữa), nên "return" im lặng ở đây sẽ giữ quạt nguyên trạng thái
  // cũ vĩnh viễn: chuồng kính nóng dần mà quạt thì đang TẮT. fail-safe = bật hết
  // gió, vì thừa mát chỉ tốn điện còn thiếu mát thì hại cây và hại cả mạch.
  if (!sys.sensorHealthy) {
    if (!sFanFailSafeWarned) {
      sFanFailSafeWarned = true;
      LOG_ERROR_CODE(204, "Temp sensor unhealthy, fan ON (fail-safe)");
    }
#if FAN_PWM_ENABLED
    pidReset();
    setFanPwm(255);
#else
    setFan(true);
#endif
    return;
  }
  sFanFailSafeWarned = false;   // cảm biến hồi phục: lần hỏng sau lại báo một lần

  const float tempHigh = sys.tempThresholdHigh;
  const float tempLow = sys.tempThresholdLow;
  // Chống ngưỡng bị đảo (sẽ làm relay đóng ngắt liên tục)
  if (tempHigh <= tempLow) return;

#if FAN_PWM_ENABLED
  // PID điều tốc: chỉ chạy khi nhiệt độ vượt ngưỡng cao; giữ nguyên tốc độ
  // trong dải trễ để quạt không tắt/bật liên tục. Dưới ngưỡng thấp thì tắt.
  if (sys.gTemp < tempLow) {
    pidReset();
    setFanPwm(0);
  } else if (sys.gTemp > tempHigh) {
    // pidUpdate(setpoint, measured) trả dương khi measured > setpoint.
    float out = pidUpdate(tempHigh, sys.gTemp);
    if (out > 0.0f && out < 80.0f) out = 80.0f;  // tốc độ tối thiểu để quạt khởi động
    uint8_t duty = (uint8_t)constrain(out, 0.0f, 255.0f);
    setFanPwm(duty);
  }
#else
  if (sys.gTemp > tempHigh && !sys.fanState) {
    setFan(true);
  } else if (sys.gTemp < tempLow && sys.fanState) {
    setFan(false);
  }
#endif
}

// ── Đèn xanh dương: ánh sáng môi trường với hysteresis (ngưỡng ± 50) ──
// Blue LED dùng chung với hiệu ứng thở, PIR và điều khiển thủ công:
// bỏ qua điều khiển tự động khi một trong số đó đang giữ LED, để tránh tranh nhau.
// Chỉ phụ thuộc cảm biến ánh sáng nên không gate theo sensorHealthy (cờ DHT).
void controlLight() {
  if (!sys.lightAuto || sys.breatheLedState || sys.gPir) return;
  const int thr = sys.lightThreshold;
  if (sys.gLight < (thr - LIGHT_HYSTERESIS) && !sys.blueLedState) {
    setBlueLed(true);
  } else if (sys.gLight > (thr + LIGHT_HYSTERESIS) && sys.blueLedState) {
    setBlueLed(false);
  }
}

// ── RGB: đèn tăng trưởng cây khi trời tối (bài 9), tắt khi trời sáng ──
// Không lưu trạng thái: so sánh với màu RGB thực tế nên màu thủ công
// không bao giờ bị bỏ quên khi bật lại chế độ tự động.
// Trạng thái "đang hiển thị hoa văn" lấy từ cờ sPlantPattern (nguồn sự thật
// của dải LED) chứ không suy đoán từ RGB: nếu người dùng tự đặt màu đơn
// (255,0,255) thì màu đó trùng giá trị RGB của hoa văn — suy đoán kiểu cũ sẽ
// tưởng hoa văn đang bật và không bật đèn tăng trưởng khi trời tối.
void controlRgb() {
  if (!sys.rgbAuto) return;
  const int thr = sys.lightThreshold;
  bool isDark = sys.gLight < (thr - LIGHT_HYSTERESIS);
  bool isBright = sys.gLight > (thr + LIGHT_HYSTERESIS);
  bool plantOn = rgbPlantPatternActive();
  bool rgbOff = (sys.rgbR == 0 && sys.rgbG == 0 && sys.rgbB == 0);
  if (isDark && !plantOn) {
    setRgbPlant();
  } else if (isBright && !rgbOff) {
    setRgb(0, 0, 0);
  }
}

// ── LED thở: tăng giảm PWM mượt trong khi sys.breatheLedState bật ──
void controlBreathe() {
  if (!sys.breatheLedState) {
    if (sys.breatheBrightness != 0) {
      sys.breatheBrightness = 0;
      setLedPwm(0);
    }
    return;
  }

  unsigned long now = millis();
  if (now - sys.lastBreatheUpdate < BREATHE_INTERVAL) return;
  sys.lastBreatheUpdate = now;

  int16_t next = (int16_t)sys.breatheBrightness + (int16_t)sys.breatheDirection * BREATHE_STEP;
  if (next >= BREATHE_MAX) {
    next = BREATHE_MAX;
    sys.breatheDirection = -1;
  } else if (next <= 0) {
    next = 0;
    sys.breatheDirection = 1;
  }
  sys.breatheBrightness = (uint8_t)next;
  setLedPwm(sys.breatheBrightness);
}

// ── Chạm: bật/tắt blue LED thủ công, mỗi lần nhấn đổi một lần (bài 4) ──
void controlTouch() {
  // GPIO39 thuộc nhóm chân chỉ-input của ESP32 classic và KHÔNG có điện kéo nội
  // (pinMode INPUT_PULLDOWN bị bỏ qua im lặng). Khi module chạm chưa cắm, đường
  // line trôi và digitalRead() có thể trả HIGH liên tục. Vì sPrevTouch khởi điểm
  // là false, mức cao đó bị hiểu là một cú nhấn ở vòng loop đầu tiên: đèn tự động
  // bị tắt, LED bật oan, và saveSettings() ghi flash — đo được trên Wokwi tại
  // 2,173 s sau mỗi lần cấp điện. Trong cửa sổ ổn định, bám theo trạng thái thật
  // để không tạo biên giả; sau cửa sổ, nếu đường line vẫn cao thì đó là mức nền
  // chứ không phải phép nhấn.
  if (millis() < INPUT_SETTLE_MS) {
    sPrevTouch = sys.gTouch;
    return;
  }
  if (sys.gTouch && !sPrevTouch) {
    unsigned long now = millis();
    if (now - sys.lastTouch >= TOUCH_DEBOUNCE) {
      if (sys.lightAuto) {  // thao tác thủ công tắt auto để tránh tranh nhau
        sys.lightAuto = false;
        saveSettings();
      }
      setBlueLed(!sys.blueLedState);
      sys.lastTouch = now;
      LOG_INFO("Touch: LED toggle");
    }
  }
  sPrevTouch = sys.gTouch;
}

// ── Cảnh báo: buzzer liên tục khi nước thấp (ưu tiên) + đèn đêm PIR ──
// Phần an toàn (nước cạn + còi thủ công) tách riêng khỏi hiệu ứng đèn PIR vì
// chế độ demo dừng controlAlerts() để không tranh LED, nhưng bể cạn thì còi
// vẫn phải kêu — trước đây demo nuốt luôn cảnh báo an toàn suốt thời gian chạy.
void controlSafetyAlerts() {
  const bool waterLow = sys.gWater < (int)sys.waterThresholdLow;
  if (waterLow) {
    setBuzzer(true);
    if (!sWaterAlarm) {
      sWaterAlarm = true;
      LOG_ERROR_CODE(302, "Low water alarm");
      notifyEvent("water", "Smart Farm: mực nước thấp, đã tắt bơm!");
    }
  } else if (sWaterAlarm) {
    sWaterAlarm = false;
    if (!sManualAlarm) setBuzzer(false);
  }
  if (sManualAlarm) setBuzzer(true);
}

bool safetyAlarmActive() {
  return sManualAlarm || sys.gWater < (int)sys.waterThresholdLow;
}

void controlAlerts() {
  controlSafetyAlerts();

  // PIR (bài 5): bật đèn khi phát hiện chuyển động, tắt khi hết chuyển động.
  // Cảnh báo ghi nhớ nó phải là thứ đã bật LED không, nên đèn đang sáng
  // (thủ công hay tự động) không bị người đi qua tắt mất. Khi hết
  // chuyển động, đèn thở đã bật trước đó (bài 3, kể cả bật thủ công qua
  // /toggle) được khôi phục thay vì bị dập tắt theo sys.breatheAuto.
  if (sys.gPir && !sMotionAlarm) {
    sMotionAlarm = true;
    sPirLitLed = !sys.blueLedState;
    sPirPrevBreathe = sys.breatheLedState;
    if (sPirLitLed) setBlueLed(true);
    LOG_ERROR_CODE(301, "Motion detected");
    notifyEvent("motion", "Smart Farm: phát hiện chuyển động!");
  } else if (!sys.gPir && sMotionAlarm) {
    sMotionAlarm = false;
    if (sPirLitLed) {
      sPirLitLed = false;
      if (sPirPrevBreathe || sys.breatheLedState) setBreatheLed(true);
      else setBlueLed(false);
    }
  }
}

// ── Cối xay gió (bài 6): quét servo qua lại như cánh quạt đang quay ──
static int sWindmillAngle = 0;
static int8_t sWindmillDir = 1;
static unsigned long sWindmillUpdate = 0;

void setWindmill(bool on) {
  if (sys.windmillOn == on) return;
  sys.windmillOn = on;
  sys.oledDirty = true;
#if WINDMILL_MOTOR_PIN >= 0
  // Động cơ thật (HĐ6) qua transistor/relay; mức HIGH = chạy nếu active-high.
  halDigitalWrite(WINDMILL_MOTOR_PIN, WINDMILL_MOTOR_ACTIVE_HIGH ? on : !on);
#endif
#if WINDMILL_SERVO_SIM
  if (on) {
    stopFeed();
    sWindmillAngle = WINDMILL_MIN_ANGLE;
    sWindmillDir = 1;
    sWindmillUpdate = 0;
  } else {
    halServoWrite(WINDMILL_MIN_ANGLE);
  }
#else
  (void)on;
#endif
  LOG_INFO(on ? "Windmill: ON" : "Windmill: OFF");
}

void controlWindmill() {
#if WINDMILL_SERVO_SIM
  if (!sys.windmillOn) return;
  unsigned long now = millis();
  if (now - sWindmillUpdate < WINDMILL_INTERVAL) return;
  sWindmillUpdate = now;
  sWindmillAngle += sWindmillDir * WINDMILL_STEP;
  if (sWindmillAngle >= WINDMILL_MAX_ANGLE) { sWindmillAngle = WINDMILL_MAX_ANGLE; sWindmillDir = -1; }
  if (sWindmillAngle <= WINDMILL_MIN_ANGLE) { sWindmillAngle = WINDMILL_MIN_ANGLE; sWindmillDir = 1;  }
  halServoWrite(sWindmillAngle);
#endif
}

// ── Cửa cho ăn (bài 10): servo mở bát trong FEED_DURATION rồi đóng lại ──
static bool sFeedActive = false;
static unsigned long sFeedStart = 0;

void startFeed() {
  setWindmill(false);            // servo dùng chung
  sys.oledDirty = true;          // dòng trạng thái OLED đổi ("Cho an...")
  sFeedActive = true;
  sFeedStart = millis();
  halServoWrite(FEED_OPEN_ANGLE);   // cửa mở
  LOG_INFO("Feed: gate open");
}

void stopFeed() {
  if (!sFeedActive) return;      // không có cho ăn thì không đụng servo
  sFeedActive = false;
  sys.oledDirty = true;          // dòng trạng thái OLED về lại
  // Đóng cửa máng ăn: servo dùng chung với cối xay gió nên nếu chỉ ngừng
  // mà không đóng lại, cửa có thể bị kẹt mở khi servo chạy sang mục khác.
  halServoWrite(FEED_CLOSE_ANGLE);
  LOG_INFO("Feed: gate closed");
}

void controlFeed() {
  if (!sFeedActive) return;
  if (millis() - sFeedStart >= FEED_DURATION) stopFeed();
}

bool feedRunning() { return sFeedActive; }

void setManualAlarm(bool on) {
  sManualAlarm = on;
  setBuzzer(on);
}

bool manualAlarm() { return sManualAlarm; }

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: đưa các biến static về trạng thái vừa cấp điện
// để mỗi test độc lập với thứ tự chạy.
void testResetControlStatics() {
  sWaterAlarm = false;
  sMotionAlarm = false;
  sManualAlarm = false;
  sPirLitLed = false;
  sPirPrevBreathe = false;
  sPrevTouch = false;
  sWindmillAngle = 0;
  sWindmillDir = 1;
  sWindmillUpdate = 0;
  sFeedActive = false;
  sFeedStart = 0;
}
#endif
