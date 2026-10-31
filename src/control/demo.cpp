#include "../globals.h"
#include "../logger.h"
#include "../actuators.h"
#include "../control.h"
#include "../demo.h"
#include "../hal.h"

// Trình diễn không chặn các bài của kit (2-12). Khi demo chạy,
// loop() chính tạm dừng tự động hóa thường để không tranh chấp chân.
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
      if (sys.gWater >= (int)sys.waterThresholdLow) setRelay(true);
      else LOG_WARN("Demo: low water, skip pump");
      break;
    case DA_PUMP_OFF:    setRelay(false); break;
    case DA_BEEP:        setBuzzer(true); break;
    case DA_PLANT_ON:    setRgbPlant(); break;
    case DA_RGB_OFF:     setRgb(0, 0, 0); break;
    case DA_WINDMILL:    setWindmill(true); break;
    case DA_SERVO0:      setWindmill(false); halServoWrite(WINDMILL_MIN_ANGLE); stopFeed(); break;
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
  halServoWrite(FEED_CLOSE_ANGLE);   // cho ăn dở phải đóng cửa trước khi demo tiếp quản servo
  sRunning = true;
  sStep = 0;
  sStepStart = millis();
  sSweepUpdate = 0;
  LOG_INFO("===== DEMO START =====");
  demoLogStep();
  demoAction(STEPS[sStep].in);
}

void updateDemo() {
  if (!sRunning) return;
  // Còi báo nước cạn / còi thủ công không được im lặng trong demo.
  controlSafetyAlerts();
  // Cửa máng ăn tự đóng theo FEED_DURATION ngay cả khi đang demo: trước đây
  // bước DA_FEED không được canh giờ nên cửa mở suốt 3 s của bước (thay vì 2 s).
  controlFeed();
  unsigned long now = millis();
  unsigned long elapsed = now - sStepStart;

  // HD6: quét servo như cánh cối xay gió đang quay. Chỉ quét khi cối xay gió
  // còn sở hữu servo: lệnh thủ công (/servo, /feed) đã setWindmill(false),
  // nếu vẫn quét thì servo giật về quỹ đạo demo và đè lên lệnh của người dùng.
  if (STEPS[sStep].in == DA_WINDMILL && sys.windmillOn &&
      now - sSweepUpdate >= WINDMILL_INTERVAL) {
    sSweepUpdate = now;
    int span = WINDMILL_MAX_ANGLE - WINDMILL_MIN_ANGLE;
    int phase = (int)(elapsed % 2000);
    int angle = WINDMILL_MIN_ANGLE + ((phase < 1000) ? (phase * span) / 1000
                                                     : ((2000 - phase) * span) / 1000);
    halServoWrite(angle);
  }

  if (elapsed < STEPS[sStep].ms) return;

  demoAction(STEPS[sStep].out);
  // Chỉ tắt còi "hét thử" của HD8; nước cạn hoặc còi thủ công đang kêu thì
  // giữ nguyên — cảnh báo an toàn thắng trình diễn.
  if (STEPS[sStep].in == DA_BEEP && !safetyAlarmActive()) setBuzzer(false);

  sStep++;
  if (sStep >= STEP_COUNT) {
    sRunning = false;
    setWindmill(false);
    setBlueLed(false);
    setBreatheLed(false);
    setRelay(false);
    if (!safetyAlarmActive()) setBuzzer(false);
    setRgb(0, 0, 0);
    halServoWrite(WINDMILL_MIN_ANGLE);
    stopFeed();
    if (sys.breatheAuto) setBreatheLed(true);   // khôi phục chế độ thở đã bật trước đó (bài 3)
    LOG_INFO("===== DEMO DONE =====");
    return;
  }

  sStepStart = now;
  demoLogStep();
  demoAction(STEPS[sStep].in);
  sys.oledDirty = true;      // dòng trạng thái OLED hiện tên bước mới
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: đưa trình diễn về trạng thái vừa cấp điện.
void testResetDemoStatics() {
  sRunning = false;
  sStep = 0;
  sStepStart = 0;
  sSweepUpdate = 0;
}
#endif
