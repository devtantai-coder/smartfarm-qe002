/*
 * SmartFarm.ino - QE002 ACEBOTT IoT Smart Farm
 * Điểm bắt đầu: setup() + loop()
 * Non-blocking, được watchdog bảo vệ, debug theo từng module
 * Tương thích với esp32 Arduino core 2.x và 3.x
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ArduinoOTA.h>

#include "src/config.h"
#include "src/globals.h"
#include "src/logger.h"
#include "src/sensors.h"
#include "src/actuators.h"
#include "src/control.h"
#include "src/settings.h"
#include "src/oled.h"
#include "src/wifi.h"
#include "src/webserver.h"
#include "src/demo.h"
#include "src/hal.h"
#include "src/calibration.h"
#include "src/anomaly.h"
#include "src/pid.h"
#include "src/softstart.h"
#include "src/mqtt.h"
#include "src/websocket.h"
#include "src/tls.h"
#include "src/ota.h"
#include "src/metrics.h"
#include "src/health.h"
#ifndef SMARTFARM_TEST_HOOKS
#include <esp_sleep.h>
#endif

// ── Trạng thái debug ──
static char sSerialCmd = 0;
static bool sDebugMode = false;

// ── Thời gian timeout watchdog (ms) ──
static constexpr uint32_t WDT_TIMEOUT_MS = 30000;

// ── Khởi động lại không chặn ──
// Sau khi lưu cài đặt WiFi cần chờ phản hồi HTTP gửi xong mới restart;
// thời gian chờ (RESTART_DELAY_MS, trong src/config.h) được đếm trong loop()
// thay vì delay().
static bool sRestartArmed = false;
static unsigned long sRestartMs = 0;
// Thời điểm bắt đầu vòng loop hiện tại (cache millis() để runAutomation và các
// xử lý trong loop không gọi millis() nhiều lần).
static unsigned long gLoopNow = 0;

static void handleFavicon() {
  server.send(204, "text/plain", "");
}

// ── Cấu hình WiFi động (định nghĩa trong webserver.cpp) ──
void handleWifi();

static void printDebugMenu() {
  Serial.println(F("\n=== SMART FARM DEBUG MENU ==="));
  Serial.println(F(" 1:Sensors  2:Actuators  3:Control"));
  Serial.println(F(" 4:Web      5:WiFi        6:OLED"));
  Serial.println(F(" 7:Settings 8:Logger      9:Demo"));
  Serial.println(F(" 0:Exit"));
  Serial.print(F("Chon: "));
}

static void handleDebugCmd(char cmd) {
  switch (cmd) {
    case '1': debugSensors(); break;
    case '2': debugActuators(); break;
    case '3': debugControl(); break;
    case '4': debugWebserver(); break;
    case '5': debugWifi(); break;
    case '6': debugOled(); break;
    case '7': debugSettings(); break;
    case '8': debugLogger(); break;
    case '0':
      stopDebugActuators();   // dừng trình tự test cơ cấu (nếu đang chạy) + tắt an toàn
      sDebugMode = false;
      Serial.println(F(">> Exit debug."));
      break;
    // loop() đã lọc ký tự trước khi gọi hàm này (chỉ '0'-'8'), nên không cần default
  }
  if (cmd != '0') printDebugMenu();
}


#if defined(BLUETOOTH_ENABLED) && BLUETOOTH_ENABLED
// Xử lý một lệnh Bluetooth hoàn chỉnh. Tách riêng để vòng loop không bị
// readStringUntil() chặn cho đến khi có '\n' (có thể tới timeout 1 s).
static void processBluetoothCommand(const String& raw) {
  String cmd = raw;
  cmd.trim();
  cmd.toLowerCase();
  if (cmd == "on" || cmd == "led on") {
    setBlueLed(true); sys.lightAuto = false;
    SerialBT.println("LED ON");
  } else if (cmd == "off" || cmd == "led off") {
    setBlueLed(false); sys.lightAuto = false;
    SerialBT.println("LED OFF");
  } else if (cmd == "pump on") {
    if (sys.gWater >= (int)sys.waterThresholdLow) { setRelay(true); sys.pumpAuto = false; SerialBT.println("Pump ON"); }
    else SerialBT.println("Low water - pump blocked");
  } else if (cmd == "pump off") {
    setRelay(false); sys.pumpAuto = false;
    SerialBT.println("Pump OFF");
  } else if (cmd == "fan on") {
    setFan(true); sys.fanAuto = false;
    SerialBT.println("Fan ON");
  } else if (cmd == "fan off") {
    setFan(false); sys.fanAuto = false;
    SerialBT.println("Fan OFF");
  } else if (cmd == "windmill on") {
    setWindmill(true);
    SerialBT.println("Windmill ON");
  } else if (cmd == "windmill off") {
    setWindmill(false);
    SerialBT.println("Windmill OFF");
  } else if (cmd == "status") {
    SerialBT.printf("T:%.1f H:%.1f L:%d S:%d W:%d\n", sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  } else if (cmd == "help") {
    SerialBT.println("Commands: on/off, pump on/off, fan on/off, windmill on/off, status, help");
  } else if (cmd.length() > 0) {
    SerialBT.println("Unknown command. Type 'help'.");
  }
}
#endif

void setup() {
  Serial.begin(115200);
  calibrationInit();
  anomalyInit();
  pidInit(1.0f, 0.0f, 0.0f);
  softStartInit();
  mqttInit();
  websocketInit();
  tlsInit();
  otaInit();
  metricsInit();

  // Watchdog (khởi tạo tương thích core 2.x / 3.x)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms = WDT_TIMEOUT_MS;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic = true;
  esp_task_wdt_deinit();
  esp_task_wdt_init(&wdtCfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
#endif
  esp_task_wdt_add(NULL);

  LOG_INFO("Smart Farm starting...");

  // GPIO: ghi sẵn mức ra chân TRƯỚC khi gọi pinMode để không bị nhiễu (glitch) lúc khởi động
  // (relay active-LOW sẽ bật chớp một cái nếu bị kéo xuống LOW trước).
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);  // tắt bơm khi khởi động
  digitalWrite(FAN_PIN, FAN_ACTIVE_LOW ? HIGH : LOW);      // tắt quạt khi khởi động
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);

  // Servo PHẢI attach trước PWM LED xanh. Trên core 3.x, ledcAttach() cấp kênh
  // LEDC trống thấp nhất (kênh 0) và ESP32Servo cũng bắt đầu cấp kênh từ 0.
  // Nếu LED chiếm kênh 0 trước, servo bị gắn nhầm vào kênh 1000 Hz của LED:
  // cối xay gió và cửa cho ăn không quay/mở được. Attach servo trước để servo
  // giữ kênh 0, LED nhận kênh 1.
  servo.attach(SERVO_PIN);
  servo.write(sys.servoAngle);

  // PWM cho LED xanh dương: bật/tắt ổn định + hiệu ứng thở (core 2.x / 3.x)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BLUE_LED_PIN, 1000, 8);
#else
  ledcSetup(LED_PWM_CHANNEL, 1000, 8);
  ledcAttachPin(BLUE_LED_PIN, LED_PWM_CHANNEL);
#endif
  setLedPwm(0);

  // Core 2.x: tone() mặc định chiếm kênh LEDC 0 (trùng kênh servo), làm servo
  // giật/không giữ góc mỗi khi còi kêu. Chọn kênh riêng cho còi; kênh 15 đã
  // dành cho LED xanh nên dùng kênh 12.
#if BUZZER_PASSIVE && ESP_ARDUINO_VERSION_MAJOR < 3
  setToneChannel(12);
#endif

  // Thiết bị ngoại vi
  dht.begin();
  strip.begin();
  strip.show();

  // OLED: I2C 400 kHz (SSD1306 hỗ trợ tối đa 400 kHz) — display() đẩy cả
  // framebuffer 1024 byte nên tăng tốc I2C giúp giảm mạnh thời gian chặn loop.
  // oledInit() tự dò màn hình và trả false nếu không có: khi đó firmware
  // không đụng I2C nữa, tránh mỗi lần vẽ phải chờ hết thời gian chờ I2C.
  if (!oledInit()) {
    LOG_ERROR_CODE(101, "OLED not found");
  }

  // Nạp cài đặt từ flash
  loadSettings();
  // Ép vòng loop đầu đọc cảm biến TRƯỚC khi automation chạy. Nếu WiFi kết nối
  // nhanh (< 1 s), nếu không ép thì controlAlerts/controlLight/controlRgb sẽ
  // hành động theo giá trị 0 khởi tạo: hú còi "hết nước" và chớp đèn/RGB oan.
  sys.lastSensorRead = millis() - SENSOR_INTERVAL;
  LOG_INFO("Settings loaded");
  if (sys.breatheAuto) setBreatheLed(true);

  // WiFi không chặn: begin() rồi để loop() xử lý. manageWiFi() tự in IP khi
  // kết nối được, tự thử lại mỗi WIFI_CHECK_INTERVAL, và báo lỗi 102 sau
  // WIFI_CONNECT_TIMEOUT nếu vẫn chưa vào mạng.
  // persistent(false): credentials đã lưu trong Preferences của firmware;
  // nếu để mặc định, mỗi WiFi.begin() (kể cả vòng thử lại mỗi 10 giây) sẽ
  // ghi thêm một lần vào NVS — hao chu kỳ ghi flash vô ích.
  WiFi.persistent(false);
  WiFi.setSleep(sys.powerSave);  // modem sleep khi tiết kiệm điện
  if (sys.ssid[0] == '\0') {
    LOG_WARN("WiFi SSID empty: copy src/secrets.h.example to src/secrets.h");
  }
  WiFi.begin(sys.ssid, sys.password);
  sys.lastWifiCheck = millis();
  LOG_INFO("Connecting WiFi...");

  // OTA: cập nhật firmware qua mạng (Arduino IDE). Không bật khi chưa có
  // mật khẩu để tránh một cổng cập nhật không xác thực trong LAN.
  ArduinoOTA.setHostname("smartfarm");
  if (sys.otaPassword[0] != '\0') {
    ArduinoOTA.setPassword(sys.otaPassword);
  }
  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA: start"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\nOTA: end"));
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    // total có thể nhỏ hơn 100 (ảnh firmware rất nhỏ) -> chia cho (total/100)
    // sẽ là chia cho 0. Tính theo phép nhân rồi chia một lần cho an toàn.
    unsigned int pct = total ? (progress * 100u) / total : 0u;
    Serial.printf("OTA: %u%%\r", pct);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA: error[%u] ", error);
    if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
  });
  if (sys.otaPassword[0] != '\0') {
    ArduinoOTA.begin();
    LOG_INFO("OTA ready");
  } else {
    LOG_WARN("OTA disabled: set OTA_PASSWORD in src/secrets.h");
  }

  // Các route của web server
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.on("/toggle", handleToggle);
  server.on("/auto", handleAuto);
  server.on("/rgb", handleRgb);
  server.on("/servo", handleServo);
  server.on("/breathe", handleBreatheLed);
  server.on("/on", handleLedOn);
  server.on("/off", handleLedOff);
  server.on("/windmill", handleWindmill);
  server.on("/feed", handleFeed);
  server.on("/demo", handleDemo);
  server.on("/threshold", handleThreshold);
  server.on("/wifi", handleWifi);
  server.on("/favicon.ico", handleFavicon);
  server.on("/health", handleHealth);
  server.on("/calibrate", handleCalibrate);
  server.on("/metrics", handleMetrics);
  // WebServer chỉ giữ các header đã đăng ký (mặc định chỉ vài header trình duyệt);
  // phải khai báo để guardWeb() đọc được Origin/Referer/Sec-Fetch-Site/X-Auth-Token.
  static const char *watchedHeaders[] = {"Origin", "Referer", "Sec-Fetch-Site", "X-Auth-Token"};
  server.collectHeaders(watchedHeaders, 4);
  server.begin();

#if defined(BLUETOOTH_ENABLED) && BLUETOOTH_ENABLED
  SerialBT.begin("SmartFarm");
  LOG_INFO("Bluetooth: SmartFarm");
#endif

  LOG_INFO("System ready");
  printDebugMenu();
}

// Toàn bộ vòng automation của loop(), tách thành hàm riêng để bộ test trên
// host gọi ĐÚNG hàm này thay vì sao chép lại logic (một nguồn sự thật duy
// nhất). loop() gọi hàm này sau khi xử lý serial/khởi động lại.

void runAutomation() {
  unsigned long now = gLoopNow ? gLoopNow : millis();

  // Đầu vào số nhanh: touch/PIR phải phản hồi nhanh hơn chu kỳ đọc cảm biến 1 giây
  sys.gTouch = readTouch();
  sys.gPir = readPIR();

  // Đọc cảm biến định kỳ (DHT11 cần >= 1 giây giữa hai lần đọc)
  if (now - sys.lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    anomalyUpdate();   // chấm điểm bất thường trên mẫu vừa lọc
    logSensors();      // mỗi giây một dòng ở mức DEBUG (activity 7)
    sys.lastSensorRead = now;
  }

  // An toàn: bơm tuyệt đối không được chạy khô, ở MỌI chế độ (thủ công hay demo)
  if (sys.gWater < (int)sys.waterThresholdLow && sys.relayState) setRelay(false);

  // Logic tự động (tạm dừng khi demo của bài học đang chạy)
  if (demoRunning()) {
    updateDemo();
    controlBreathe();   // giữ bước thở HD3 chạy liên tục trong lúc demo
  } else {
    controlPump();
    controlFan();
    controlLight();
    controlRgb();
    controlBreathe();
    controlTouch();
    controlAlerts();
    controlWindmill();
  }
  controlFeed();
}

void loop() {
  esp_task_wdt_reset();
  gLoopNow = millis();

  // Một vòng loop duy nhất cho cả production và test: web, WiFi, OTA và
  // automation dùng chung dữ liệu, không còn task/queue/mutex song song.
  server.handleClient();
  manageWiFi();
  mqttLoop();
  websocketLoop();
  otaLoop();

  // ArduinoOTA.handle() đọc một gói UDP mỗi lần gọi. OTA không nhạy cảm với
  // độ trễ (bắt tay OTA mất hàng trăm ms) nên gọi mỗi OTA_HANDLE_INTERVAL là
  // quá đủ — tiết kiệm ~50 lần recvfrom mỗi giây, không ảnh hưởng gì.
  static unsigned long sLastOtaHandle = 0;
  if (gLoopNow - sLastOtaHandle >= OTA_HANDLE_INTERVAL) {
    sLastOtaHandle = gLoopNow;
    ArduinoOTA.handle();
  }

#if defined(BLUETOOTH_ENABLED) && BLUETOOTH_ENABLED
  // Bluetooth serial command handling — không chặn: đọc từng byte có sẵn,
  // tích lũy đến '\n' rồi mới xử lý, tránh readStringUntil() chờ timeout.
  {
    static String sBtBuf;
    while (SerialBT.available() > 0) {
      char c = (char)SerialBT.read();
      if (c == '\n' || c == '\r') {
        if (sBtBuf.length() > 0) {
          processBluetoothCommand(sBtBuf);
          sBtBuf = "";
        }
      } else if (sBtBuf.length() < 64) {
        sBtBuf += c;
      }
    }
    // Lệnh không có newline: xử lý khi buffer đầy để không kẹt mãi.
    if (sBtBuf.length() >= 64) {
      processBluetoothCommand(sBtBuf);
      sBtBuf = "";
    }
  }
#endif

  // Khởi động lại sau khi lưu cài đặt WiFi/mật khẩu (chờ phản hồi HTTP xong).
  // Không dùng delay(): đếm đủ RESTART_DELAY_MS trong loop rồi mới restart.
  if (sys.gNeedRestart) {
    if (!sRestartArmed) {
      sRestartArmed = true;
      sRestartMs = gLoopNow;
    } else if (gLoopNow - sRestartMs >= RESTART_DELAY_MS) {
      ESP.restart();
    }
  } else {
    sRestartArmed = false;
  }

  // Nhập lệnh debug qua Serial
  if (Serial.available() > 0) {
    sSerialCmd = Serial.read();
    if (sSerialCmd == '\n' || sSerialCmd == '\r') {
      // bỏ qua
    } else if (!sDebugMode) {
      if (sSerialCmd == '9') {
        startDemo();
      } else if (sSerialCmd >= '0' && sSerialCmd <= '8') {
        sDebugMode = true;
        handleDebugCmd(sSerialCmd);
      } else {
        Serial.println(F(">> Invalid. Enter 0-9."));
        printDebugMenu();
      }
    } else {
      if (sSerialCmd == '0') {
        handleDebugCmd('0');
        Serial.println(F("Running normally..."));
      } else {
        Serial.println(F(">> Enter '0' to exit debug."));
      }
    }
  }

  if (sDebugMode) {
    // Bơm trình tự test cơ cấu (bấm '2') — chạy không chặn, web/OTA vẫn phục vụ
    updateDebugActuators();
    // Chế độ debug dừng automation, nhưng cửa cho ăn vẫn phải tự đóng sau
    // FEED_DURATION (nếu mở từ web trong lúc debug) để không kẹt mở mãi.
    controlFeed();
    return;
  }

  runAutomation();

  if (sys.oledDirty || (gLoopNow - sys.lastOledUpdate >= OLED_INTERVAL)) {
    updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
    sys.lastOledUpdate = gLoopNow;
    sys.oledDirty = false;
  }

  // Tiết kiệm điện: sau cửa sổ thức (30 s) cho phép truy cập web, vào deep sleep
  // theo sleepSeconds rồi tự tỉnh dậy bằng timer / chân cảm ứng / PIR.
#ifndef SMARTFARM_TEST_HOOKS
  if (sys.powerSave && !demoRunning() && !sDebugMode && gLoopNow >= 30000UL) {
    esp_sleep_enable_timer_wakeup((uint64_t)sys.sleepSeconds * 1000000ULL);
    // ext1 bao phủ cả chân chạm (39) và PIR (33); không dùng ext0 cùng lúc
    // vì hai nguồn external-wakeup xung đột trên ESP32.
    esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_PIN) | (1ULL << PIR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
    LOG_INFO("Power save: entering deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
  }
#endif
}
