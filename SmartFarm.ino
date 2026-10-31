/*
 * SmartFarm.ino - QE002 ACEBOTT IoT Smart Farm
 * Điểm bắt đầu: setup() + loop()
 * Non-blocking, được watchdog bảo vệ, debug theo từng module
 * Tương thích với esp32 Arduino core 2.x và 3.x
 */

#include <Arduino.h>
#include <ctype.h>
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
#include "src/ota.h"
#include "src/metrics.h"
#include "src/health.h"
#include "src/connectivity/history.h"
#include "src/connectivity/weather.h"
#include "src/connectivity/notify.h"
#include "src/connectivity/networker.h"
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

// ── Cấu hình WiFi động + factory reset (định nghĩa trong webserver.cpp,
//    khai báo trong src/webserver.h) ──

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
// Nhận const char* và chuẩn hóa trong buffer tĩnh: không cấp phát String.
static void processBluetoothCommand(const char* raw) {
  if (!raw) return;
  char cmd[65];
  size_t n = 0;
  const char* p = raw;
  while (*p != '\0' && isspace((unsigned char)*p)) p++;
  while (*p != '\0' && n < sizeof(cmd) - 1) {
    cmd[n++] = (char)tolower((unsigned char)*p++);
  }
  while (n > 0 && isspace((unsigned char)cmd[n - 1])) n--;
  cmd[n] = '\0';

  if (strcmp(cmd, "on") == 0 || strcmp(cmd, "led on") == 0) {
    setBlueLed(true); sys.lightAuto = false;
    SerialBT.println("LED ON");
  } else if (strcmp(cmd, "off") == 0 || strcmp(cmd, "led off") == 0) {
    setBlueLed(false); sys.lightAuto = false;
    SerialBT.println("LED OFF");
  } else if (strcmp(cmd, "pump on") == 0) {
    if (sys.gWater >= (int)sys.waterThresholdLow) { setRelay(true); sys.pumpAuto = false; SerialBT.println("Pump ON"); }
    else SerialBT.println("Low water - pump blocked");
  } else if (strcmp(cmd, "pump off") == 0) {
    setRelay(false); sys.pumpAuto = false;
    SerialBT.println("Pump OFF");
  } else if (strcmp(cmd, "fan on") == 0) {
    setFan(true); sys.fanAuto = false;
    SerialBT.println("Fan ON");
  } else if (strcmp(cmd, "fan off") == 0) {
    setFan(false); sys.fanAuto = false;
    SerialBT.println("Fan OFF");
  } else if (strcmp(cmd, "windmill on") == 0) {
    setWindmill(true);
    SerialBT.println("Windmill ON");
  } else if (strcmp(cmd, "windmill off") == 0) {
    setWindmill(false);
    SerialBT.println("Windmill OFF");
  } else if (strcmp(cmd, "feed") == 0) {
    startFeed();
    SerialBT.println("Feeding");
  } else if (strcmp(cmd, "demo") == 0) {
    startDemo();
    SerialBT.println("Demo started");
  } else if (strcmp(cmd, "alarm on") == 0) {
    setManualAlarm(true);
    SerialBT.println("Alarm ON");
  } else if (strcmp(cmd, "alarm off") == 0) {
    setManualAlarm(false);
    SerialBT.println("Alarm OFF");
  } else if (strncmp(cmd, "rgb ", 4) == 0) {
    int r = 0, g = 0, b = 0;
    if (sscanf(cmd + 4, "%d %d %d", &r, &g, &b) == 3) {
      setRgbAuto(false);
      setRgb((uint8_t)constrain(r, 0, 255), (uint8_t)constrain(g, 0, 255),
             (uint8_t)constrain(b, 0, 255));
      SerialBT.println("RGB set");
    } else {
      SerialBT.println("Usage: rgb R G B");
    }
  } else if (strncmp(cmd, "door ", 5) == 0 || strncmp(cmd, "servo ", 6) == 0) {
    int angle = 0;
    sscanf(cmd, strncmp(cmd, "door ", 5) == 0 ? "door %d" : "servo %d", &angle);
    angle = constrain(angle, 0, 180);
    setWindmill(false);
    stopFeed();
    sys.servoAngle = angle;
    servo.write(angle);
    SerialBT.printf("Servo %d\n", angle);
  } else if (strcmp(cmd, "status") == 0) {
    SerialBT.printf("T:%.1f H:%.1f L:%d S:%d W:%d\n", sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
  } else if (strcmp(cmd, "help") == 0) {
    SerialBT.println("Commands: on/off, pump on/off, fan on/off, windmill on/off,");
    SerialBT.println("          rgb R G B, door/servo 0-180, feed, demo, alarm on/off, status, help");
  } else if (cmd[0] != '\0') {
    SerialBT.println("Unknown command. Type 'help'.");
  }
}

#if defined(SMARTFARM_TEST_HOOKS)
void testProcessBluetoothCommand(const char* raw) {
  processBluetoothCommand(raw ? raw : "");
}
#endif
#endif

void setup() {
#ifndef SMARTFARM_TEST_HOOKS
  setCpuFrequencyMhz(CPU_FREQ_MHZ);
#endif
  Serial.begin(115200);
  calibrationInit();
  anomalyInit();
  pidInit(1.0f, 0.0f, 0.0f);
  softStartInit();
  historyInit();
  weatherInit();
  notifyInit();
  // Task nền cho MỌI I/O mạng ra ngoài (Telegram, thời tiết): loop() chỉ xếp
  // job, không bao giờ chờ TLS/HTTP.
  netWorkerInit();
  mqttInit();
  otaInit();
  metricsInit();

  // Watchdog (khởi tạo tương thích core 2.x / 3.x). Giá trị trả về PHẢI được kiểm
  // tra: trước đây cả hai nhánh đều bị bỏ qua nên nếu WDT không nhận mức 30 s thì
  // firmware im lặng chạy với timeout cũ của core (ngắn hơn nhiều). So với 0 thay
  // vì ESP_OK vì bộ test trên host mock esp_task_wdt mà không có esp_err.h.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms = WDT_TIMEOUT_MS;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic = true;
  // IDF 5.x: esp_task_wdt_init() trả ESP_ERR_INVALID_STATE nếu WDT đã tồn tại,
  // nên phải gỡ bản tạo lúc boot trước khi tạo lại với timeout mới.
  esp_task_wdt_deinit();
  if (esp_task_wdt_init(&wdtCfg) != 0) {
    LOG_ERROR_CODE(104, "WDT init failed - timeout stays at core default");
  }
#else
  // IDF 4.4: esp_task_wdt_init(timeout, panic) CẬP NHẬT tại chỗ timeout/panic của
  // WDT đã được tạo lúc boot (không phải ESP_ERR_INVALID_STATE như IDF 5.x), nên
  // gọi thẳng là đủ; deinit trước sẽ fail vì idle task còn theo dõi -> mất watchdog.
  if (esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true) != 0) {
    LOG_ERROR_CODE(104, "WDT init failed - timeout stays at core default");
  }
#endif
  // ESP_ERR_INVALID_ARG nếu core đã subscribe loopTask từ trước: loop vẫn được
  // giám sát, chỉ không phải do ta đăng ký -> WARN, không phải lỗi.
  if (esp_task_wdt_add(NULL) != 0) {
    LOG_WARN("WDT subscribe failed - loop may be unwatched");
  }

  LOG_INFO("Smart Farm starting...");

  // GPIO: ghi sẵn mức ra chân TRƯỚC khi gọi pinMode để không bị nhiễu (glitch) lúc khởi động
  // (relay active-LOW sẽ bật chớp một cái nếu bị kéo xuống LOW trước).
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);  // tắt bơm khi khởi động
  digitalWrite(FAN_PIN, FAN_ACTIVE_LOW ? HIGH : LOW);      // tắt quạt khi khởi động
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
#if WINDMILL_MOTOR_PIN >= 0
  digitalWrite(WINDMILL_MOTOR_PIN, WINDMILL_MOTOR_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(WINDMILL_MOTOR_PIN, OUTPUT);
#endif
  // PIR (33) là GPIO thông thường nên kéo xuống trong firmware được: không có
  // module PIR cắm vào thì chân không trôi, tránh "phát hiện chuyển động" oan.
  // TOUCH (39) thuộc nhóm chỉ-input của ESP32 classic, KHÔNG có điện kéo nội
  // (pinMode INPUT_PULLDOWN bị bỏ qua im lặng) — cần trở kéo xuống ngoài 10k
  // xuống GND nếu module chạm không kéo được đường line về thấp; firmware tự
  // bọc bằng cửa sổ ổn định + gieo trạng thái nền (controlTouch).
  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN, INPUT_PULLDOWN);

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

#if FAN_PWM_ENABLED
  // Quạt điều tốc bằng PWM (tùy chọn). Gắn sau servo/LED để không chiếm kênh 0.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(FAN_PIN, 1000, 8);
#else
  ledcSetup(FAN_PWM_CHANNEL, 1000, 8);
  ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
#endif
  setFanPwm(0);
#endif

  // Core 2.x: tone() mặc định chiếm kênh LEDC 0 (trùng kênh servo), làm servo
  // giật/không giữ góc mỗi khi còi kêu. Chọn kênh riêng cho còi; kênh 15 đã
  // dành cho LED xanh nên dùng kênh 12 (kênh 13 trùng LEDC timer với 12 nên quạt
  // đã được đẩy sang kênh 11 — xem ràng buộc kênh trong src/config.h).
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
  // Modem sleep mặc định tắt để web phản hồi nhanh; bật bằng WIFI_MODEM_SLEEP
  // hoặc khi chọn chế độ tiết kiệm điện (deep sleep).
  WiFi.setSleep(WIFI_MODEM_SLEEP || sys.powerSave);  // modem sleep khi tiết kiệm điện
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
  // PHẢI truyền HTTP_GET tường minh. Overload 2 tham số của WebServer là
  // HTTP_ANY (core 3.3.12: WebServer.cpp "on(uri, handler)" -> on(uri, HTTP_ANY,
  // ...)), nên mọi route nhận cả POST. Với POST/PUT/PATCH/DELETE, core đọc body
  // NGAY TRONG server.handleClient(), tức trên loopTask, và
  // readBytesWithTimeout() đặt lại biến đếm timeout ở MỖI vòng lặp
  // (Parsing.cpp:47-53): một client gửi 1 byte mỗi ~4 s giữ loopTask kẹt vô hạn,
  // watchdog 30 s panic reboot. Vì loop() dừng nên khóa an toàn "cấm bơm chạy
  // khô" (runAutomation) cũng dừng theo — đây là lỗi an toàn vật lý, không chỉ
  // là web chậm. Trang web và app ACEBOTT đều chỉ gọi GET kèm query string
  // (/door?level=0/1), nên giới hạn method không đổi hành vi nào.
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api", HTTP_GET, handleApi);
  server.on("/toggle", HTTP_GET, handleToggle);
  server.on("/auto", HTTP_GET, handleAuto);
  server.on("/rgb", HTTP_GET, handleRgb);
  server.on("/servo", HTTP_GET, handleServo);
  server.on("/breathe", HTTP_GET, handleBreatheLed);
  server.on("/on", HTTP_GET, handleLedOn);
  server.on("/off", HTTP_GET, handleLedOff);
  server.on("/windmill", HTTP_GET, handleWindmill);
  server.on("/feed", HTTP_GET, handleFeed);
  server.on("/demo", HTTP_GET, handleDemo);
  server.on("/threshold", HTTP_GET, handleThreshold);
  server.on("/wifi", HTTP_GET, handleWifi);
  server.on("/reset", HTTP_GET, handleReset);
  server.on("/favicon.ico", HTTP_GET, handleFavicon);
  server.on("/health", HTTP_GET, handleHealth);
  server.on("/calibrate", HTTP_GET, handleCalibrate);
  server.on("/metrics", HTTP_GET, handleMetrics);
  // Tương thích app ACEBOTT QE002 (hoạt động 13)
  server.on("/config", HTTP_GET, handleAppConfig);
  server.on("/all", HTTP_GET, handleAppAll);
  server.on("/door", HTTP_GET, handleAppDoor);
  server.on("/led", HTTP_GET, handleAppLed);
  // Lịch sử + thời tiết Internet
  server.on("/history", HTTP_GET, handleHistory);
  server.on("/weather", HTTP_GET, handleWeather);
  // WebServer chỉ giữ các header đã đăng ký (mặc định chỉ vài header trình duyệt);
  // phải khai báo để guardWeb() đọc được Origin/Referer/Sec-Fetch-Site/X-Auth-Token;
  // User-Agent dùng để phát hiện request do trình duyệt tạo nhưng đã giấu nguồn (CSRF).
  // If-None-Match dùng cho ETag/304 của trang chủ.
  static const char *watchedHeaders[] = {"Origin", "Referer", "Sec-Fetch-Site", "X-Auth-Token", "User-Agent", "If-None-Match"};
  server.collectHeaders(watchedHeaders, 6);
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

// An toàn vô điều kiện: bơm tuyệt đối không được chạy khô. Hàm riêng vì nó phải
// chạy ở MỌI chế độ — cả debug, nơi runAutomation() bị loop() bỏ qua.
static void pumpDryRunGuard() {
  if (sys.gWater < (int)sys.waterThresholdLow) {
    if (sys.relayState) setRelay(false);
    softStartCancel();
  }
}

void runAutomation() {
  unsigned long now = gLoopNow ? gLoopNow : millis();

  // Đầu vào số nhanh: touch/PIR phải phản hồi nhanh hơn chu kỳ đọc cảm biến
  sys.gTouch = readTouch();
  sys.gPir = readPIR();

  // Đọc cảm biến định kỳ (nhịp lấy theo mức tối thiểu DHT phục vụ — SENSOR_INTERVAL)
  if (now - sys.lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    anomalyUpdate();   // chấm điểm bất thường trên mẫu vừa lọc
    logSensors();      // một dòng ở mức DEBUG mỗi chu kỳ đọc (activity 7)
    sys.lastSensorRead = now;
  }

  // An toàn: bơm tuyệt đối không được chạy khô, ở MỌI chế độ (thủ công hay demo)
  pumpDryRunGuard();

  // Logic tự động (tạm dừng khi demo hoặc khi app ACEBOTT vừa điều khiển)
  if (demoRunning()) {
    softStartCancel();
    updateDemo();
    controlBreathe();   // giữ bước thở HD3 chạy liên tục trong lúc demo
  } else if (appControlActive()) {
    // App đang điều khiển trực tiếp: chỉ giữ cảnh báo an toàn, không giành quyền.
    softStartCancel();
    controlAlerts();
  } else {
    controlPump();
    softStartUpdate();
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
  const uint32_t loopStartUs = micros();
  esp_task_wdt_reset();
  gLoopNow = millis();

  // Một vòng loop duy nhất cho cả production và test: web, WiFi, OTA và
  // automation dùng chung dữ liệu. Chỉ Telegram/thời tiết chạy trên task mạng
  // nền (networker.cpp) để TLS không chặn loop; dữ liệu chia sẻ có IrqLock.
  server.handleClient();
  manageWiFi();
  mqttLoop();
  otaLoop();
  weatherLoop(gLoopNow);

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
  // Buffer char cố định thay cho String: không cấp phát heap mỗi ký tự.
  {
    static char sBtBuf[65];
    static uint8_t sBtLen = 0;
    while (SerialBT.available() > 0) {
      char c = (char)SerialBT.read();
      if (c == '\n' || c == '\r') {
        if (sBtLen > 0) {
          sBtBuf[sBtLen] = '\0';
          processBluetoothCommand(sBtBuf);
          sBtLen = 0;
        }
      } else if (sBtLen < 64) {
        sBtBuf[sBtLen++] = c;
      }
    }
    // Lệnh không có newline: xử lý khi buffer đầy để không kẹt mãi.
    if (sBtLen >= 64) {
      sBtBuf[sBtLen] = '\0';
      processBluetoothCommand(sBtBuf);
      sBtLen = 0;
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
    // Debug dừng automation nhưng KHÔNG được dừng phần an toàn: vẫn phải đọc cảm
    // biến để đại lượng nuôi interlock còn sống (bể cạn trong lúc debug sẽ bị phát
    // hiện), và phải kiểm tra sau khi trình tự test ghi cơ cấu.
    if (gLoopNow - sys.lastSensorRead >= SENSOR_INTERVAL) {
      readAllSensors();
      sys.lastSensorRead = gLoopNow;
    }
    // Bơm trình tự test cơ cấu (bấm '2') — chạy không chặn, web/OTA vẫn phục vụ
    updateDebugActuators();
    // Chế độ debug dừng automation, nhưng cửa cho ăn vẫn phải tự đóng sau
    // FEED_DURATION (nếu mở từ web trong lúc debug) để không kẹt mở mãi.
    controlFeed();
    pumpDryRunGuard();   // đặt SAU hai bước trên để cắt ngay relay vừa bị bật
    metricsNoteLoop(micros() - loopStartUs);
    return;
  }

  runAutomation();
  historyLoop(gLoopNow);

  if (sys.oledDirty || (gLoopNow - sys.lastOledUpdate >= OLED_INTERVAL)) {
    updateOLED(sys.gTemp, sys.gHum, sys.gLight, sys.gSoil, sys.gWater);
    sys.lastOledUpdate = gLoopNow;
    sys.oledDirty = false;
  }

  // Tiết kiệm điện: sau cửa sổ thức (POWER_SAVE_AWAKE_MS) cho phép truy cập web,
  // vào deep sleep theo sleepSeconds rồi tự tỉnh dậy bằng timer / chân cảm ứng / PIR.
  // Lưu ý: mỗi lần tỉnh dậy firmware chạy lại setup() — lịch sử RAM mất, NVS đọc
  // lại, servo attach lại — nên đừng đặt cửa sổ quá ngắn nếu cần dashboard/OTA.
#ifndef SMARTFARM_TEST_HOOKS
  if (sys.powerSave && !demoRunning() && !sDebugMode && gLoopNow >= POWER_SAVE_AWAKE_MS) {
    esp_sleep_enable_timer_wakeup((uint64_t)sys.sleepSeconds * 1000000ULL);
    // ext1 bao phủ cả chân chạm (39) và PIR (33); không dùng ext0 cùng lúc
    // vì hai nguồn external-wakeup xung đột trên ESP32.
    esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_PIN) | (1ULL << PIR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
    LOG_INFO("Power save: entering deep sleep");
    Serial.flush();
    esp_deep_sleep_start();
  }
#endif

  metricsNoteLoop(micros() - loopStartUs);
}
