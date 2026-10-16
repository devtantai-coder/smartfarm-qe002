# GIẢI THÍCH CODE TỪNG DÒNG — SMART FARM QE002 ACEBOTT

Tài liệu này dành cho **người chưa từng học lập trình** cũng đọc hiểu được. Mỗi đoạn code
đều được trích ra và giải thích ngay bên dưới: dòng đó làm gì, vì sao cần nó.

> Mẹo đọc: mở file code thật bên cạnh (ví dụ `SmartFarm.ino`) rồi đối chiếu số dòng.
> Code trong tài liệu được trích nguyên văn từ repo.

---

## PHẦN 0 — BỨC TRANH TỔNG THỂ (đọc trước khi vào chi tiết)

### 0.1. Hệ thống này là gì?

Đây là **firmware** (phần mềm nhúng) cho bộ kit "Smart Farm QE002 ACEBOTT" — một mô hình
vườn thông minh dùng chip **ESP32**. Firmware chạy liên tục trên chip và làm 3 việc song song:

1. **Đọc cảm biến** — nhiệt độ/độ ẩm (DHT11), ánh sáng, độ ẩm đất, mực nước, chuyển động (PIR), cảm ứng chạm.
2. **Ra quyết định** — ví dụ: đất khô thì bật bơm, nóng quá thì bật quạt, tối thì bật đèn…
3. **Giao tiếp** — mở trang web để điện thoại/máy tính điều khiển, hiện thông tin lên màn hình OLED, nhận lệnh qua Serial Monitor.

Một điểm rất quan trọng: **mọi việc đều "không chặn" (non-blocking)**. Nghĩa là firmware
không bao giờ dùng `delay()` dài trong vòng lặp chính; nó chỉ *xem đồng hồ* rồi quyết định
"đã đến lúc làm việc X chưa". Nhờ vậy web vẫn phản hồi, nút bấm vẫn nhạy, cảm biến vẫn đọc
đều — dù đang chạy trình diễn 30 giây.

### 0.2. Cấu trúc thư mục

```
duan123/
├── SmartFarm.ino        ← "bộ não": hàm setup() chạy 1 lần khi bật, loop() chạy mãi mãi
├── src/
│   ├── config.h/.cpp    ← Bản đồ chân GPIO + thông số thời gian + ngưỡng mặc định
│   ├── globals.h/.cpp   ← Các biến "toàn cục" (ai cũng đọc/ghi được) + đối tượng phần cứng
│   ├── logger.h/.cpp    ← Ghi log ra Serial + 8 hàm debug cho menu serial
│   ├── sensors.h/.cpp   ← Đọc 6 cảm biến + lọc nhiễu (trung bình động)
│   ├── actuators.h/.cpp ← Bật/tắt bơm, quạt, còi, đèn, RGB, servo
│   ├── control.h/.cpp   ← "Luật chơi" tự động: khi nào bật/tắt cái gì
│   ├── demo.h/.cpp      ← Trình diễn 15 bước HOẠT ĐỘNG 2→12
│   ├── oled.h/.cpp      ← Vẽ 8 dòng thông tin lên màn hình OLED
│   ├── settings.h/.cpp  ← Lưu/đọc cài đặt vào bộ nhớ flash (không mất khi tắt điện)
│   ├── wifi.h/.cpp      ← Giữ WiFi luôn kết nối, tự thử lại mỗi 10 giây
│   └── webserver.h/.cpp ← 14 "cửa" (route) web + trang HTML điều khiển
└── test/                ← Bộ test chạy trên máy tính, không cần kit thật
```

### 0.3. Luồng hoạt động (vẽ đơn giản)

```
BẬT ĐIỆN
   │
   ▼
setup()  ── chạy MỘT LẦN ──────────────────────────────
   │  • bật watchdog (còi báo nếu chip "đơ")
   │  • khai báo các chân vào/ra (đèn, bơm, quạt…)
   │  • khởi động DHT11, dải LED RGB, servo, màn hình OLED
   │  • đọc cài đặt đã lưu trong flash
   │  • kết nối WiFi (chờ tối đa 30 giây)
   │  • đăng ký 14 đường dẫn web rồi mở "quầy tiếp tân" (server.begin)
   ▼
loop()  ── chạy LẶP MÃI (hàng nghìn lần mỗi giây) ─────
   │  1. "đút thức ăn" cho watchdog (esp_task_wdt_reset)
   │  2. phục vụ khách web (server.handleClient)
   │  3. kiểm tra WiFi còn sống không (manageWiFi)
   │  4. đọc phím bấm từ Serial Monitor
   │  5. đọc cảm biến theo nhịp 1 giây
   │  6. LUẬT AN TOÀN: nước thấp → bơm phải tắt
   │  7. nếu trình diễn đang chạy → chạy bước tiếp theo
   │     ngược lại → chạy bộ luật tự động (bơm/quạt/đèn/chạm/còi…)
   │  8. cập nhật màn hình OLED mỗi 2 giây
   ▼
(lặp lại từ đầu, khoảng vài nghìn lần mỗi giây)
```

### 0.4. Bảng chân GPIO (lấy từ `src/config.h`)

| Chân GPIO | Nối với | Kiểu | Ghi chú |
|---|---|---|---|
| 23 | Cảm biến DHT11 | Đọc | Nhiệt độ + độ ẩm không khí |
| 19 | Dải 4 LED RGB (NeoPixel) | Đọc/ghi | "Đèn tăng trưởng" HOẠT ĐỘNG 9 |
| 13 | Servo SG90 | Ghi | Cánh cối xay gió + cửa máng ăn (dùng chung 1 servo) |
| 25 | Còi buzzer | Ghi | Kêu khi nước cạn (HOẠT ĐỘNG 8) |
| 26 | Relay máy bơm | Ghi | Bật/tắt bơm (HOẠT ĐỘNG 7) |
| 14 | Quạt (qua relay) | Ghi | Tản nhiệt |
| 27 | LED xanh dương | Ghi (PWM) | Vừa "bật/tắt" vừa "thở" mượt |
| 39 | Cảm biến chạm | Đọc | Chạm → đổi trạng thái LED (HOẠT ĐỘNG 4) |
| 33 | Cảm biến PIR | Đọc | Phát hiện người (HOẠT ĐỘNG 5) |
| 34 | Quang trở LDR | Đọc (ADC) | Đo độ sáng |
| 35 | Cảm biến độ ẩm đất | Đọc (ADC) | Đất khô ↔ ẩm |
| 36 | Cảm biến mực nước | Đọc (ADC) | Bể cạn ↔ đầy |
| 21 / 22 | OLED SDA / SCL | Giao tiếp I2C | Màn hình 128×64 |
| 15 | Kênh PWM cho LED xanh (core 2.x) | — | Tránh đụng timer với servo |

### 0.5. Từ vựng nhanh

- **GPIO**: chân cắm trên chip, có thể là "cửa vào" (đọc cảm biến) hoặc "cửa ra" (điều khiển thiết bị).
- **PWM**: kỹ thuật bật/tắt chân cực nhanh để giả độ sáng (0 = tắt, 255 = sáng hết cỡ).
- **ADC**: bộ chuyển đổi điện áp → con số 0…4095 (chip ESP32 có 12 bit).
- **Ngưỡng (threshold)**: con số mốc để ra quyết định, ví dụ "đất < 400 thì bật bơm".
- **Hysteresis (trễ)**: dùng 2 ngưỡng khác nhau cho bật và tắt để thiết bị không bật–tắt liên tục quanh mốc.
- **Chống dội (debounce)**: bỏ qua các tín hiệu bấm quá nhanh, tránh 1 lần bấm bị tính thành nhiều lần.
- **Watchdog**: "đồng hồ báo thức" của chip — nếu phần mềm đơ quá 30 giây, chip tự khởi động lại.
- **Static**: biến chỉ khởi tạo 1 lần, giữ giá trị giữa các lần gọi hàm.
- **Route**: một địa chỉ URL cụ thể trên web, ví dụ `/on` là "công tắc bật đèn".

---

## PHẦN 1 — SmartFarm.ino (bộ não, 229 dòng)

### 1.1. Khối chú thích đầu file (dòng 1–6)

```cpp
/*
 * SmartFarm.ino - QE002 ACEBOTT IoT Smart Farm
 * Entry point: setup() + loop()
 * Non-blocking, watchdog-protected, modular debug
 * Compatible with esp32 Arduino core 2.x and 3.x
 */
```

- Dòng 1–6: chú thích mô tả dự án. Arduino chỉ chạy file `.ino`; đây là "cửa vào" của firmware.
- "Entry point" = nơi chương trình bắt đầu. "Non-blocking" = không dùng delay dài. "modular debug" = menu debug chia theo mô-đun.

### 1.2. Khai báo thư viện (dòng 8–24)

```cpp
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
```

- Dòng 8: `Arduino.h` cho các hàm nền tảng (`pinMode`, `digitalWrite`, `millis()`, `Serial`…).
- Dòng 9: `WiFi.h` để kết nối WiFi.
- Dòng 10: `WebServer.h` để làm web server.
- Dòng 11: `Preferences.h` để đọc/ghi bộ nhớ flash (giữ cài đặt khi mất điện).
- Dòng 12: `esp_task_wdt.h` để dùng watchdog của ESP32.

```cpp
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
```

- Dòng 14–24: nạp toàn bộ mô-đun của dự án. Mỗi file `.h` chứa "lời hứa" (khai báo hàm), file `.cpp` chứa "thân hàm" (code thật).

### 1.3. Trạng thái debug (dòng 26–28)

```cpp
static char sSerialCmd = 0;
static bool sDebugMode = false;
```

- Dòng 27: biến giữ ký tự vừa đọc từ Serial Monitor ("phím" người dùng gõ).
- Dòng 28: `sDebugMode = true` nghĩa là đang trong menu debug — lúc đó bộ luật tự động tạm dừng để bạn thử tay từng thiết bị. Chữ `s` ở đầu là quy ước "static" (biến nội bộ file này).

### 1.4. Thời gian watchdog (dòng 30–31)

```cpp
static constexpr uint32_t WDT_TIMEOUT_MS = 30000;
```

- Dòng 31: watchdog sẽ reset chip nếu quá 30.000 ms (30 giây) mà phần mềm không "đút thức ăn". Vòng lặp `loop()` bình thường chạy vài nghìn lần mỗi giây nên không bao giờ chạm mốc này — chỉ khi code bị treo thật.

### 1.5. Hàm phục vụ favicon (dòng 33–35)

```cpp
static void handleFavicon() {
  server.send(204, "text/plain", "");
}
```

- Dòng 34: khi trình duyệt tự hỏi `/favicon.ico` (biểu tượng tab), ta trả mã **204 = "No Content"** (không có gì). Trả lời nhanh gọn để trình duyệt không tự gọi lại nhiều lần. Đây là route thứ 14.

### 1.6. In menu debug (dòng 37–44)

```cpp
static void printDebugMenu() {
  Serial.println(F("\n=== SMART FARM DEBUG MENU ==="));
  Serial.println(F(" 1:Sensors  2:Actuators  3:Control"));
  Serial.println(F(" 4:Web      5:WiFi        6:OLED"));
  Serial.println(F(" 7:Settings 8:Logger      9:Demo"));
  Serial.println(F(" 0:Exit"));
  Serial.print(F("Chon: "));
}
```

- Dòng 38–42: in bảng hướng dẫn ra Serial Monitor. Chữ `F("...")` bọc chuỗi để chuỗi được cất trong **flash** thay vì RAM — ESP32 RAM ít, làm vậy tiết kiệm.
- Dòng 43: `print` (không xuống dòng) để con trỏ nằm ngay sau chữ "Chon: ", người dùng gõ số ngay đó.

### 1.7. Xử lý phím debug (dòng 46–60)

```cpp
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
    case '0': sDebugMode = false; Serial.println(F(">> Exit debug.")); break;
    default:  Serial.println(F(">> Invalid.")); break;
  }
  if (cmd != '0') printDebugMenu();
}
```

- Dòng 47–55: `switch` = "bảng tra": gõ phím nào thì gọi hàm debug tương ứng. Mỗi hàm này in thông tin và/hoặc kích hoạt thiết bị thật (ví dụ '2' cho bơm/quạt/đèn chạy thử 1 nhịp).
- Dòng 56: phím '0' đặt `sDebugMode = false` → thoát chế độ debug, in lời nhắn.
- Dòng 57: phím lạ → in ">> Invalid.".
- Dòng 59: sau khi chạy lệnh (trừ thoát), in lại menu để người dùng chọn tiếp.

### 1.8. setup() — chạy một lần khi bật nguồn (dòng 62–158)

```cpp
void setup() {
  Serial.begin(115200);
  delay(100);
```

- Dòng 63: mở cổng Serial ở tốc độ 115200 baud để nói chuyện với Serial Monitor.
- Dòng 64: chờ 100 ms cho cổng ổn định. Đây là `delay` duy nhất ngoài lúc chờ WiFi — lúc này chưa cần phản hồi nhanh.

```cpp
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
```

- Dòng 67: `#if` là "biên dịch có điều kiện". Thư viện ESP32 có 2 phiên bản lớn (core 2.x và 3.x) với cách gọi watchdog khác nhau. Đoạn này tự chọn đúng API, nhờ vậy cùng một code chạy được ở cả hai.
- Dòng 68–72 (core 3.x): tạo cấu hình watchdog: hết hạn 30 giây, không giám sát core rảnh, hết hạn thì reset chip. `deinit()` trước `init()` để xóa cấu hình cũ (tránh lỗi "đã khởi tạo rồi").
- Dòng 75 (core 2.x): gọi hàm cũ với đơn vị giây.
- Dòng 77: "đăng ký" task hiện tại (chính là vòng lặp chính) vào watchdog. Từ đây, mỗi vòng `loop()` phải gọi `esp_task_wdt_reset()`.

```cpp
  LOG_INFO("Smart Farm starting...");
```

- Dòng 79: ghi log khởi động qua hệ thống logger của dự án.

```cpp
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);  // pump OFF at boot
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);
```

- Đây là mẹo rất quan trọng: **viết mức điện trước khi khai báo chân là ngõ ra**. Vì sao? Nếu bạn khai báo `OUTPUT` trước, chân có thể tự phát ra một xung điện rất ngắn (glitch) — với relay loại "kích mức thấp" (active-LOW), xung đó sẽ làm bơm nháy chạy khi vừa bật điện. Ghi mức "tắt" trước, rồi mới chuyển thành OUTPUT → không có glitch.
- Dòng 83: relay bơm ghi mức tắt. `RELAY_ACTIVE_LOW` là cờ cấu hình: module relay loại kích mức thấp thì mức tắt là HIGH, loại thường là LOW. Toán tử `? :` đọc là "nếu … thì … ngược lại …".
- Dòng 84–85: quạt và còi cũng ghi mức tắt trước.
- Dòng 86–90: chính thức khai báo 3 chân output và 2 chân input (chạm, PIR).

```cpp
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BLUE_LED_PIN, 1000, 8);
#else
  ledcSetup(LED_PWM_CHANNEL, 1000, 8);
  ledcAttachPin(BLUE_LED_PIN, LED_PWM_CHANNEL);
#endif
  setLedPwm(0);
```

- Dòng 93–97: mở PWM cho LED xanh: tần số 1000 Hz, độ phân giải 8 bit (0…255). Core 3.x gắn PWM trực tiếp theo chân; core 2.x phải chọn "kênh" (ở đây là kênh 15, cố ý chọn khác kênh servo để 2 thứ không tranh nhau timer).
- Dòng 99: đặt độ sáng 0 (tắt) lúc khởi động.

```cpp
  dht.begin();
  strip.begin();
  strip.show();
  servo.attach(SERVO_PIN);
  servo.write(servoAngle);
```

- Dòng 102: khởi động cảm biến DHT11.
- Dòng 103–104: khởi động dải LED RGB và `show()` ngay để tắt hết LED (xóa trạng thái rác).
- Dòng 105–106: gắn servo vào chân 13 và đưa về góc 0 (góc "đóng").

```cpp
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    LOG_ERROR_CODE(101, "OLED not found");
  }
  display.clearDisplay();
  display.display();
```

- Dòng 109: mở bus I2C với 2 chân 21 (dữ liệu) và 22 (nhịp).
- Dòng 110: thử khởi động màn hình OLED. Nếu `begin` trả về `false` (không tìm thấy màn hình), ghi mã lỗi **101** — mạch vẫn chạy tiếp bình thường, chỉ là không có màn hình.
- Dòng 113–114: xóa màn hình và hiển thị (màn hình trắng trơn).

```cpp
  loadSettings();
  LOG_INFO("Settings loaded");
  if (breatheAuto) setBreatheLed(true);
```

- Dòng 117: `loadSettings()` đọc các ngưỡng + cờ tự động từ flash (nếu chưa từng lưu thì dùng mặc định).
- Dòng 119: nếu người dùng đã bật "đèn thở" từ lần trước, bật lại ngay — vì trạng thái bật/tắt thật của LED không lưu vào flash, chỉ lưu *cờ cho phép*.

```cpp
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  LOG_INFO("Connecting WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    esp_task_wdt_reset();
    delay(250);
    Serial.print('.');
  }
```

- Dòng 122: tắt chế độ "ngủ" của WiFi để phản hồi web nhanh.
- Dòng 123: bắt đầu kết nối bằng tên mạng + mật khẩu trong `config.cpp` (bạn phải sửa 2 dòng này thành WiFi nhà bạn).
- Dòng 125: ghi lại mốc thời gian bắt đầu chờ.
- Dòng 126: lặp trong khi **chưa kết nối được** và **chưa quá 30 giây**. Cách viết `millis() - start` an toàn cả khi đồng hồ tràn số (xem giải thích ở Phần 8).
- Dòng 127: "đút thức ăn" cho watchdog mỗi vòng chờ — vì đây là vòng lặp có delay, vẫn phải chứng minh phần mềm còn sống.
- Dòng 128: chờ 250 ms mỗi vòng (tổng tối đa 30 giây = 120 lần lặp).
- Dòng 129: in dấu chấm để bạn thấy tiến trình trên Serial Monitor.

```cpp
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    LOG_ERROR_CODE(102, "WiFi timeout - retrying in background");
  }
```

- Dòng 131: xuống dòng sau dấu chấm.
- Dòng 132–134: nếu kết nối thành công, in địa chỉ IP để bạn gõ lên trình duyệt.
- Dòng 135–136: nếu hết 30 giây vẫn chưa nối được, ghi mã lỗi **102** và tiếp tục thử lại "ngầm" (hàm `manageWiFi()` trong `loop()` lo việc này).

```cpp
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
  server.on("/favicon.ico", handleFavicon);
  server.begin();
```

- Dòng 140–153: "đăng ký" 14 đường dẫn web. Mỗi dòng nghĩa là: khi có người mở địa chỉ này, hãy gọi hàm xử lý kia. Ví dụ mở `http://<IP>/?id=pump` của `/toggle` sẽ gọi `handleToggle()`.
- Dòng 154: mở "quầy tiếp tân" — từ đây server mới bắt đầu nhận khách.

```cpp
  LOG_INFO("System ready");
  printDebugMenu();
}
```

- Dòng 156–157: ghi log "sẵn sàng" và in menu debug lần đầu.

### 1.9. loop() — chạy lặp mãi mãi (dòng 160–229)

```cpp
void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  manageWiFi();
```

- Dòng 161: "đút thức ăn" cho watchdog — bắt buộc ở đầu mỗi vòng.
- Dòng 162: xử lý mọi yêu cầu web đang chờ (nếu có).
- Dòng 163: kiểm tra WiFi, tự nối lại nếu rớt (chi tiết ở Phần 12).

```cpp
  if (Serial.available() > 0) {
    sSerialCmd = Serial.read();
    if (sSerialCmd == '\n' || sSerialCmd == '\r') {
      // skip
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
```

- Dòng 166: có ký tự người dùng gõ trên Serial Monitor không?
- Dòng 167: đọc 1 ký tự.
- Dòng 168–169: nếu là ký tự xuống dòng (`\n`, `\r`) thì bỏ qua — vì Serial Monitor thường gửi kèm.
- Dòng 170: nếu **không** trong debug:
  - Dòng 171–172: gõ `9` → chạy trình diễn 15 bước.
  - Dòng 173–175: gõ `0`…`8` → vào debug và gọi đúng hàm của phím.
  - Dòng 176–179: gõ ký tự khác → báo sai cú pháp và in lại menu.
- Dòng 180–187: nếu **đang** debug: chỉ phím `0` mới có tác dụng (thoát); phím khác nhắc lại. Đây là "khóa an toàn" — trong lúc thử tay, bạn không vô tình chạy demo.

```cpp
  if (sDebugMode) return;
```

- Dòng 190: đang debug thì **dừng vòng lặp tại đây**. Nghĩa là toàn bộ tự động hóa (bơm, quạt, còi…) tạm nghỉ để bạn thử thiết bị bằng tay. Khi thoát debug, dòng đầu tiên của lần lặp kế tiếp sẽ khôi phục luật an toàn.

```cpp
  unsigned long now = millis();

  gTouch = readTouch();
  gPir = readPIR();
```

- Dòng 192: lấy đồng hồ hiện tại (mili-giây kể từ lúc bật). Dùng chung 1 giá trị cho cả vòng để logic nhất quán.
- Dòng 195–196: đọc 2 cảm biến số **mỗi vòng lặp** (không chờ 1 giây) vì nút chạm và PIR cần phản ứng tức thì.

```cpp
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    logSensors();   // one line every second at DEBUG level (activity 7)
    lastSensorRead = now;
  }
```

- Dòng 199: đã đủ 1000 ms kể từ lần đọc trước chưa? DHT11 yêu cầu cách nhau tối thiểu 1 giây nên các cảm biến được đọc theo nhịp riêng.
- Dòng 200: đọc tất cả cảm biến + lọc nhiễu + cập nhật các biến `g…`.
- Dòng 201: in 1 dòng tóm tắt cảm biến (chỉ hiện khi mức log là DEBUG).
- Dòng 202: ghi lại mốc thời gian vừa đọc.

```cpp
  if (gWater < (int)waterThresholdLow && relayState) setRelay(false);
```

- Dòng 206: **luật an toàn số 1 của cả firmware**: nếu mực nước thấp hơn ngưỡng mà bơm đang chạy thì tắt bơm ngay lập tức. Dòng này nằm ở đây (ngoài mọi nhánh) nên nó có hiệu lực **trong mọi chế độ**: tự động, thủ công, trình diễn, debug… Bơm chạy khô sẽ cháy.

```cpp
  if (demoRunning()) {
    updateDemo();
    controlBreathe();   // keep the HD3 breathing step animating during the demo
  } else {
    controlPump();
    controlFan();
    controlLight();
    controlRgb();
    controlBreathe();
    controlTouch();
    controlAlerts();
    controlWindmill();
    controlFeed();
  }
```

- Dòng 209: nếu trình diễn đang chạy:
  - Dòng 210: chạy bước tiếp theo của trình diễn (khi nào hết giờ bước thì chuyển bước).
  - Dòng 211: vẫn cập nhật hiệu ứng "đèn thở" để bước HD3 nhấp nháy mượt.
- Dòng 212–222: ngược lại, chạy lần lượt 9 bộ luật tự động. Thứ tự có ý nghĩa:
  1. `controlPump` — bơm theo đất + an toàn nước.
  2. `controlFan` — quạt theo nhiệt độ.
  3. `controlLight` — đèn xanh theo độ sáng (nhường LED nếu đèn thở/PIR/người dùng đang giữ).
  4. `controlRgb` — "đèn tăng trưởng" theo độ sáng.
  5. `controlBreathe` — hiệu ứng thở.
  6. `controlTouch` — xử lý chạm.
  7. `controlAlerts` — còi báo nước + đèn PIR.
  8. `controlWindmill` — quét servo cối xay gió.
  9. `controlFeed` — đóng cửa máng ăn khi hết giờ.

```cpp
  if (now - lastOledUpdate >= OLED_INTERVAL) {
    updateOLED(gTemp, gHum, gLight, gSoil, gWater);
    lastOledUpdate = now;
  }
}
```

- Dòng 225: cứ 2000 ms vẽ lại màn hình một lần (vẽ thường xuyên hơn sẽ nhấp nháy và tốn thời gian).
- Dòng 226: vẽ 8 dòng thông tin (thời tiết, nhiệt độ, độ sáng, đất, nước, IP, trạng thái).
- Dòng 227: ghi mốc thời gian vừa vẽ.

---

## PHẦN 2 — src/config.h và src/config.cpp (bảng cấu hình)

### 2.1. config.h — khai báo chân, thời gian, cờ hiệu chuẩn

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
```

- Dòng 1–2: "include guard" — kỹ thuật chống nạp file 2 lần. Nếu file đã nạp rồi thì bỏ qua; tránh lỗi "khai báo trùng".
- Dòng 4: cần `Arduino.h` cho kiểu dữ liệu chuẩn.

```cpp
extern const char *ssid;
extern const char *password;
```

- Dòng 7–8: `extern` nghĩa là "biến này được định nghĩa ở file .cpp khác, đây chỉ là lời hứa". Giá trị thật nằm ở `config.cpp`.

```cpp
#define DHT_PIN          23
#define DHT_TYPE         DHT11
#define RGB_PIN          19
#define RGB_COUNT        4
#define SERVO_PIN        13
#define BUZZER_PIN       25
#define RELAY_PIN        26
#define FAN_PIN          14
#define BLUE_LED_PIN     27
#define LED_PWM_CHANNEL  15
#define TOUCH_PIN        39
#define PIR_PIN          33
#define LDR_PIN          34
#define SOIL_PIN         35
#define WATER_PIN        36
#define OLED_SDA         21
#define OLED_SCL         22
#define OLED_RESET       -1
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64
```

- Dòng 11–30: `#define` là "đặt tên cho con số". Nhờ vậy khi đọc code thấy `RELAY_PIN` là hiểu ngay, không phải nhớ "26 là chân gì". Muốn đổi dây cắm chỉ sửa đúng 1 dòng ở đây.
- `LED_PWM_CHANNEL 15`: trên core 2.x mỗi PWM cần 1 kênh; chọn kênh 15 để không trùng kênh của servo (servo dùng timer khác).
- `SCREEN_WIDTH/HEIGHT`: màn OLED 128×64 pixel.

```cpp
extern float soilThresholdLow;
extern float soilThresholdHigh;
extern float waterThresholdLow;
extern int   lightThreshold;
extern float tempThresholdHigh;
extern float tempThresholdLow;
```

- Dòng 33–38: 6 ngưỡng điều khiển. `float` = số có phần thập phân, `int` = số nguyên. Giá trị thật ở `config.cpp`, có thể thay đổi từ web và lưu vào flash.

```cpp
extern const unsigned long WIFI_CONNECT_TIMEOUT;
extern const unsigned long WIFI_CHECK_INTERVAL;
extern const unsigned long SENSOR_INTERVAL;
extern const unsigned long OLED_INTERVAL;
extern const unsigned long TOUCH_DEBOUNCE;
extern const unsigned long BREATHE_INTERVAL;
extern const unsigned long WINDMILL_INTERVAL;
extern const unsigned long FEED_DURATION;
```

- Dòng 41–48: 8 mốc thời gian (mili-giây). `unsigned long` = số nguyên dương lớn (đủ chứa số mili-giây trong ~49 ngày).
- `const` = chỉ đọc, không sửa lúc chạy.

```cpp
constexpr uint8_t SENSOR_FILTER_SIZE = 5;  // Moving average window
```

- Dòng 51: cửa sổ lọc trung bình động 5 mẫu — mỗi lần đọc cảm biến, lấy trung bình 5 giá trị gần nhất để số liệu đỡ nhảy.

```cpp
#ifndef INVERT_LIGHT
#define INVERT_LIGHT     0
#endif
#ifndef INVERT_SOIL
#define INVERT_SOIL      0
#endif
#ifndef INVERT_WATER
#define INVERT_WATER     0
#endif
```

- Dòng 58–66: 3 cờ đảo chiều cảm biến. Quy ước tài liệu: LDR sáng = số to, đất khô = số nhỏ, nước đầy = số to. Nếu module thực tế của bạn đọc ngược, chỉ cần đổi cờ này thành 1 — **không phải sửa logic**.
- Cấu trúc `#ifndef … #define … #endif` cho phép ghi đè từ dòng lệnh biên dịch (`-DINVERT_LIGHT=1`) — bộ test dùng cơ chế này để chạy cả 2 chiều.

```cpp
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 0
#endif
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE   0
#endif
#ifndef BUZZER_TONE_HZ
#define BUZZER_TONE_HZ   2000
#endif
```

- `RELAY_ACTIVE_LOW`: 1 nếu module relay của bạn loại "kích mức thấp" (bơm chạy khi chân = LOW).
- `BUZZER_PASSIVE`: 1 nếu còi của bạn là loại "thụ động" (phải phát xung nhạc bằng `tone()` mới kêu), 0 nếu là loại "chủ động" (chỉ cần cấp điện).
- `BUZZER_TONE_HZ 2000`: tần số 2000 Hz khi kêu còi thụ động.

```cpp
#ifndef WINDMILL_MIN_ANGLE
#define WINDMILL_MIN_ANGLE 0
#endif
#ifndef WINDMILL_MAX_ANGLE
#define WINDMILL_MAX_ANGLE 180
#endif
#ifndef FEED_OPEN_ANGLE
#define FEED_OPEN_ANGLE    90
#endif
#ifndef FEED_CLOSE_ANGLE
#define FEED_CLOSE_ANGLE   0
#endif

#endif
```

- Dòng 82–93: 4 góc servo để hiệu chuẩn cơ khí: cối xay gió quét từ 0° đến 180°, cửa máng ăn mở 90° / đóng 0°. Tùy cách bạn lắp cánh tay servo, có thể phải chỉnh để không bị kẹt bánh răng.
- Dòng 95: kết thúc include guard.

### 2.2. config.cpp — giá trị thật của cấu hình

```cpp
#include "config.h"
```

- Dòng 1: nạp bản khai báo (để trình biên dịch kiểm tra tên/kiểu khớp nhau).

```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
```

- Dòng 4–5: **bạn phải sửa 2 dòng này** thành tên và mật khẩu WiFi nhà bạn trước khi nạp code.

```cpp
float soilThresholdLow  = 400.0f;
float soilThresholdHigh = 700.0f;
float waterThresholdLow = 200.0f;
int   lightThreshold    = 500;
float tempThresholdHigh = 30.0f;
float tempThresholdLow  = 25.0f;
```

- Dòng 8–13: ngưỡng mặc định lúc xuất xưởng:
  - Đất: dưới 400 (khô) → bật bơm; trên 700 (đủ ẩm) → tắt bơm.
  - Nước: dưới 200 (bể cạn) → còi báo, cấm bơm.
  - Sáng: 500 (thang 0…4095) — tối hơn mức này thì bật đèn.
  - Nhiệt độ: trên 30 °C → bật quạt; dưới 25 °C → tắt quạt.
- Chữ `f` sau số (400.0f) chỉ "đây là số float" cho trình biên dịch.

```cpp
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long SENSOR_INTERVAL     = 1000;
const unsigned long OLED_INTERVAL       = 2000;
const unsigned long TOUCH_DEBOUNCE      = 300;
const unsigned long BREATHE_INTERVAL    = 30;
const unsigned long WINDMILL_INTERVAL   = 30;    // servo sweep step (activity 6)
const unsigned long FEED_DURATION       = 2000;  // feed gate open time (activity 10)
```

- Dòng 16–23: các mốc thời gian:
  - `WIFI_CONNECT_TIMEOUT` 30 giây: chờ WiFi tối đa lúc khởi động.
  - `WIFI_CHECK_INTERVAL` 10 giây: nhịp kiểm tra WiFi sau đó.
  - `SENSOR_INTERVAL` 1 giây: nhịp đọc cảm biến (DHT11 không cho đọc nhanh hơn).
  - `OLED_INTERVAL` 2 giây: nhịp vẽ màn hình.
  - `TOUCH_DEBOUNCE` 300 ms: khoảng lặng chống dội cho nút chạm.
  - `BREATHE_INTERVAL` 30 ms: mỗi 30 ms đổi độ sáng LED 5 đơn vị → hiệu ứng "thở" mượt.
  - `WINDMILL_INTERVAL` 30 ms: mỗi 30 ms quét servo 6° → cối xay quay.
  - `FEED_DURATION` 2000 ms: cửa máng ăn mở đúng 2 giây rồi đóng.

---

## PHẦN 3 — src/globals.h và src/globals.cpp (biến dùng chung)

### 3.1. globals.h — lời hứa cho mọi mô-đun

```cpp
#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <Preferences.h>
```

- Dòng 1–2: include guard như mọi file .h.
- Dòng 4–12: nạp các thư viện phần cứng. Vì các "đối tượng phần cứng" (DHT, OLED…) được khai báo ngay trong file này nên cần đủ thư viện ở đây.

```cpp
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif
```

- Dòng 15–20: dò phiên bản core Arduino-ESP32. `__has_include` hỏi trình biên dịch "có file này không?" — core 3.x có, core 2.x không. Nếu không xác định được thì mặc định coi là 2.x. Nhờ vậy các chỗ `#if ESP_ARDUINO_VERSION_MAJOR >= 3` trong toàn dự án luôn có giá trị.

```cpp
#include "config.h"
#include "logger.h"
```

- Dòng 22–23: nạp cấu hình chân + logger để dùng chung.

```cpp
extern bool pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto;
extern bool relayState, fanState, blueLedState, breatheLedState;
extern bool windmillOn;
extern uint8_t rgbR, rgbG, rgbB;
extern uint8_t breatheBrightness;
extern int8_t breatheDirection;
extern int servoAngle;
```

- Dòng 26: 5 cờ "tự động" — bật thì firmware tự quyết định, tắt thì chờ lệnh người dùng.
- Dòng 27: 4 trạng thái thật của thiết bị (bơm đang chạy?, quạt?, đèn xanh?, đèn thở?).
- Dòng 28: cối xay gió có đang quay không.
- Dòng 29: màu RGB hiện tại (0…255 mỗi kênh).
- Dòng 30–31: độ sáng hiện tại của đèn thở + hướng tăng/giảm (1 = sáng dần, -1 = mờ dần).
- Dòng 32: góc servo hiện tại.

```cpp
extern float gTemp, gHum;
extern int gLight, gSoil, gWater;
extern bool gPir, gTouch;
```

- Dòng 35: nhiệt độ và độ ẩm **đã lọc** (trung bình 5 mẫu).
- Dòng 36: độ sáng, độ ẩm đất, mực nước đã lọc.
- Dòng 37: trạng thái PIR và chạm. Chữ `g` = global (toàn cục) để phân biệt với giá trị thô.

```cpp
extern unsigned long lastWifiCheck, lastSensorRead, lastOledUpdate;
extern unsigned long lastBuzzer, lastTouch, lastBreatheUpdate;
```

- Dòng 40–41: 6 "mốc thời gian lần cuối" phục vụ các bộ đếm nhịp (WiFi, cảm biến, OLED, còi, chạm, đèn thở).

```cpp
extern DHT dht;
extern Adafruit_SSD1306 display;
extern Adafruit_NeoPixel strip;
extern Servo servo;
extern WebServer server;
extern Preferences preferences;
```

- Dòng 44–49: 6 "đối tượng phần cứng" — mỗi đối tượng là một cầu nối tới thư viện điều khiển thiết bị thật.

```cpp
extern uint8_t sensorFailCount;
extern bool sensorHealthy;

#endif
```

- Dòng 52: đếm số lần đọc cảm biến DHT bị lỗi liên tiếp.
- Dòng 53: cảm biến còn "khỏe" không (false = hỏng liên tục, đã ghi log cảnh báo).

### 3.2. globals.cpp — giá trị khởi tạo

```cpp
#include "globals.h"

bool pumpAuto = true, lightAuto = true, rgbAuto = true, fanAuto = true, breatheAuto = false;
bool relayState = false, fanState = false, blueLedState = false, breatheLedState = false;
bool windmillOn = false;
uint8_t rgbR = 0, rgbG = 0, rgbB = 0;
uint8_t breatheBrightness = 0;
int8_t breatheDirection = 1;
int servoAngle = 0;
```

- Dòng 1: nạp lời hứa — nhờ đó trình biên dịch đối chiếu từng biến có khớp không.
- Dòng 4: lúc khởi động, 4 chế độ tự động bật sẵn; riêng "đèn thở" tắt (opt-in — người dùng muốn mới bật) để màn hình không nhấp nháy ngay từ đầu.
- Dòng 5: mọi thiết bị khởi động ở trạng thái tắt — an toàn nhất.
- Dòng 7–10: màu RGB đen (tắt), độ sáng 0, hướng thở lên, servo góc 0.

```cpp
float gTemp = 0, gHum = 0;
int gLight = 0, gSoil = 0, gWater = 0;
bool gPir = false, gTouch = false;

unsigned long lastWifiCheck = 0, lastSensorRead = 0, lastOledUpdate = 0;
unsigned long lastBuzzer = 0, lastTouch = 0, lastBreatheUpdate = 0;
```

- Dòng 13–19: các giá trị cảm biến và mốc thời gian bắt đầu từ 0. Ngay vòng lặp đầu tiên, `now - 0 >= 1000` đúng → cảm biến được đọc ngay.

```cpp
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);
Servo servo;
WebServer server(80);
Preferences preferences;
```

- Dòng 22: tạo đối tượng DHT11 ở chân 23.
- Dòng 23: tạo màn hình OLED 128×64, giao tiếp qua bus `Wire`, chân reset = -1 (dùng chân reset của module, không cần chân riêng).
- Dòng 24: dải 4 LED RGB ở chân 19, thứ tự màu GRB, tốc độ 800 kHz — đúng chuẩn chip WS2812 trên kit.
- Dòng 25: servo chưa gắn chân (gắn trong `setup()` bằng `servo.attach`).
- Dòng 26: web server lắng nghe cổng 80 — cổng mặc định của HTTP.
- Dòng 27: đối tượng đọc/ghi flash.

```cpp
uint8_t sensorFailCount = 0;
bool sensorHealthy = true;
```

- Dòng 30–31: bộ đếm lỗi cảm biến bắt đầu từ 0 và coi như khỏe mạnh.

---

## PHẦN 4 — src/logger.h và src/logger.cpp (ghi log + debug)

### 4.1. logger.h — hợp đồng của hệ thống log

```cpp
#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL };
```

- Dòng 6: `enum` là "bộ hằng số có thứ tự". Máy tính lưu LOG_DEBUG = 0, LOG_INFO = 1, LOG_WARNING = 2, LOG_ERROR = 3, LOG_FATAL = 4. Thứ tự này có chủ đích: mức càng lớn càng nghiêm trọng, nên chỉ cần so sánh `<` để lọc.

```cpp
extern LogLevel gMinLogLevel;

extern bool dbgSensors, dbgActuators, dbgControl;
extern bool dbgWebserver, dbgWifi, dbgOled;
extern bool dbgSettings, dbgLogger, dbgRgb;
```

- Dòng 9: mức log tối thiểu sẽ được in. Đang để INFO nghĩa là DEBUG bị bỏ qua.
- Dòng 12–14: 9 công tắc debug theo mô-đun — chỉ in log chi tiết của mô-đun bạn quan tâm.

```cpp
void logMessage(LogLevel level, const char* func, int line, const char* msg);
void logError(LogLevel level, const char* func, int line, int code, const char* msg);
```

- Dòng 16: hàm in log thường. `func` (tên hàm) và `line` (số dòng) để biết log phát ra từ đâu.
- Dòng 17: hàm in log kèm **mã lỗi số** (ví dụ 201 = bơm chạy khô).

```cpp
void logSensors();
```

- Dòng 21: in 1 dòng tóm tắt tất cả cảm biến (chỉ khi mức log là DEBUG).

```cpp
void debugSensors();
void debugActuators();
void debugControl();
void debugWebserver();
void debugWifi();
void debugOled();
void debugSettings();
void debugLogger();
```

- Dòng 24–31: 8 hàm cho menu serial — mỗi hàm in thông tin một nhóm và/hoặc chạy thử thiết bị.

```cpp
#define LOG_DEBUG(msg)  logMessage(LOG_DEBUG,  __func__, __LINE__, msg)
#define LOG_INFO(msg)   logMessage(LOG_INFO,   __func__, __LINE__, msg)
#define LOG_WARN(msg)   logMessage(LOG_WARNING,__func__, __LINE__, msg)
#define LOG_ERROR(msg)  logMessage(LOG_ERROR,  __func__, __LINE__, msg)
#define LOG_FATAL(msg)  logMessage(LOG_FATAL,  __func__, __LINE__, msg)
#define LOG_ERROR_CODE(code, msg) logError(LOG_ERROR, __func__, __LINE__, code, msg)
```

- Dòng 33–38: `#define` dạng hàm — viết tắt cho gọn. `__func__` là tên hàm hiện tại, `__LINE__` là số dòng hiện tại; trình biên dịch tự điền, nên bạn không phải gõ tay. Nhờ vậy mọi dòng log đều tự ghi rõ "phát ra từ đâu".
- Ví dụ: `LOG_INFO("Pump: ON");` trong `setRelay` sẽ in `[...] [INFO ] setRelay:14 -> Pump: ON`.

```cpp
#define MOD_DEBUG(mod, msg)   do { if (dbg##mod) logMessage(LOG_DEBUG, __func__, __LINE__, msg); } while(0)
```

- Dòng 41: macro debug theo mô-đun. Hai dấu `##` là phép "nối chữ": `dbg##Rgb` ghép thành `dbgRgb`. `do { … } while(0)` là mẹo đóng gói nhiều lệnh thành 1 khối an toàn trong mọi câu `if`.

### 4.2. logger.cpp — phần thân

```cpp
#include "logger.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"

bool dbgSensors = false, dbgActuators = false, dbgControl = false;
bool dbgWebserver = false, dbgWifi = false, dbgOled = false;
bool dbgSettings = false, dbgLogger = false, dbgRgb = false;

LogLevel gMinLogLevel = LOG_INFO;

static bool sLogInit = false;
```

- Dòng 7–9: tất cả công tắc debug tắt sẵn (giữ Serial gọn gàng khi vận hành bình thường).
- Dòng 12: chỉ in từ mức INFO trở lên.
- Dòng 14: cờ "đã in dòng chào mừng chưa" — đảm bảo chỉ in 1 lần.

```cpp
static const char* levelTag(LogLevel l) {
  switch (l) {
    case LOG_DEBUG:   return "DEBUG";
    case LOG_INFO:    return "INFO ";
    case LOG_WARNING: return "WARN ";
    case LOG_ERROR:   return "ERROR";
    case LOG_FATAL:   return "FATAL";
    default:          return "?????";
  }
}
```

- Dòng 16–25: dịch số mức log thành chữ để in. Mẹo căn lề: "INFO " và "WARN " có thêm 1 dấu cách cho đủ 5 ký tự — nhìn cột log thẳng hàng. `default` phòng trường hợp giá trị lạ.

```cpp
static void ensureInit() {
  if (sLogInit) return;
  sLogInit = true;
  Serial.println(F("\n=== SMART FARM LOGGER ==="));
}
```

- Dòng 27–31: lần đầu có log thì in dòng phân cách. Nhờ `if (sLogInit) return;` nó không bao giờ in lại.

```cpp
static void printHeader(LogLevel level, const char* func, int line) {
  unsigned long ms = millis();
  unsigned long sec = ms / 1000;
  char stamp[40];
  snprintf(stamp, sizeof(stamp), "[%02lu:%02lu:%02lu.%03lu] [%s] ",
           (sec / 3600) % 100, (sec / 60) % 60, sec % 60, ms % 1000,
           levelTag(level));
  Serial.print(stamp);
  Serial.print(func);
  Serial.print(':');
  Serial.print(line);
}
```

- Dòng 35: lấy đồng hồ mili-giây từ lúc bật.
- Dòng 36: đổi sang giây.
- Dòng 37: tạo chuỗi "dấu thời gian" tối đa 40 ký tự.
- Dòng 38–40: `snprintf` = "in ra chuỗi có giới hạn độ dài" (an toàn, không tràn bộ nhớ). Định dạng `[HH:MM:SS.mmm] [LEVEL] `:
  - `(sec/3600)%100`: giờ (lấy phần dư 100 để không tràn khi chạy lâu).
  - `(sec/60)%60`: phút; `sec%60`: giây; `ms%1000`: phần mili-giây.
  - `%02lu` = số nguyên dài, in tối thiểu 2 chữ số (đệm 0).
- Dòng 41–44: in dấu thời gian, rồi tên hàm, dấu hai chấm, số dòng. Chuỗi sẽ nối thành: `[00:00:05.123] [INFO ] setRelay:14`.

```cpp
void logMessage(LogLevel level, const char* func, int line, const char* msg) {
  if (level < gMinLogLevel) return;
  ensureInit();
  printHeader(level, func, line);
  Serial.print(F(" -> "));
  Serial.println(msg);
}
```

- Dòng 48: nếu mức log thấp hơn mức tối thiểu thì bỏ qua — đây là "van lọc" duy nhất, giúp các macro gọi thoải mái ở mọi nơi mà không tốn thời gian in.
- Dòng 49–52: in phần đầu rồi in ` -> ` và nội dung, kết thúc bằng xuống dòng.

```cpp
void logError(LogLevel level, const char* func, int line, int code, const char* msg) {
  if (level < gMinLogLevel) return;  // fixes code being printed below min level
  ensureInit();
  printHeader(level, func, line);
  Serial.printf(" [MA LOI: %d] -> %s\n", code, msg);
}
```

- Dòng 55–60: giống `logMessage` nhưng chèn `[MA LOI: <số>]`. Mã lỗi giúp tra cứu nhanh trong tài liệu (ví dụ 201 = nước thấp ngắt bơm, 401 = mất WiFi).

```cpp
void logSensors() {
  if (LOG_DEBUG < gMinLogLevel) return;
  Serial.printf("[SENSORS] T:%.1fC H:%.1f%% Light:%d Soil:%d Water:%d PIR:%d Touch:%d\n",
                gTemp, gHum, gLight, gSoil, gWater, (int)gPir, (int)gTouch);
}
```

- Dòng 64: chỉ chạy khi mức log cho phép DEBUG.
- Dòng 65–66: in tất cả cảm biến trên 1 dòng. `%.1f` = số thực 1 chữ số thập phân; `%%` = in ký tự phần trăm (vì `%` đã dành cho định dạng); `(int)gPir` đổi true/false thành 1/0.

```cpp
void debugSensors() {
  Serial.println(F("\n--- SENSORS ---"));
  readAllSensors();
  Serial.printf("T:%.1fC  H:%.1f%%\n", gTemp, gHum);
  Serial.printf("Light:%d  Soil:%d  Water:%d\n", gLight, gSoil, gWater);
  Serial.printf("PIR:%d  Touch:%d  Health:%d\n", gPir, gTouch, sensorHealthy);
}
```

- Dòng 70–76: phím '1' của menu. Đáng chú ý: gọi `readAllSensors()` để bạn thấy số liệu **mới nhất ngay lúc đó**, kể cả chưa đến nhịp 1 giây.

```cpp
void debugActuators() {
  Serial.println(F("\n--- ACTUATORS ---"));
  Serial.println(F("Pump on/off..."));
  setRelay(true);  delay(800); setRelay(false); delay(400);
  Serial.println(F("Fan on/off..."));
  setFan(true);    delay(800); setFan(false);   delay(400);
  Serial.println(F("LED on/off..."));
  setBlueLed(true); delay(800); setBlueLed(false); delay(400);
  Serial.println(F("Breathe on/off..."));
  setBreatheLed(true); delay(1500); setBreatheLed(false); delay(400);
  Serial.println(F("RGB test..."));
  setRgb(255,0,0); delay(600);
  setRgb(0,255,0); delay(600);
  setRgb(0,0,255); delay(600);
  setRgb(0,0,0);
  Serial.println(F("RGB plant pattern..."));
  setRgbPlant(); delay(800); setRgb(0,0,0);
  Serial.println(F("Buzzer on/off..."));
  setBuzzer(true); delay(400); setBuzzer(false);
  Serial.println(F("Servo sweep..."));
  servo.write(WINDMILL_MIN_ANGLE); delay(600);
```

- Dòng 79–103: phím '2' chạy thử lần lượt từng thiết bị, mỗi thiết bị bật vài trăm ms rồi tắt. Đây là chỗ **được phép dùng delay dài** vì người dùng đang chủ động xem demo thiết bị; xong bạn bấm '0' để về chế độ thường. Đèn thở giữ 1,5 giây để mắt kịp thấy hiệu ứng.

```cpp
void debugControl() {
  Serial.println(F("\n--- CONTROL ---"));
  Serial.printf("Soil: %.0f-%.0f  Water:%.0f\n", soilThresholdLow, soilThresholdHigh, waterThresholdLow);
  Serial.printf("Light:%d  Temp:%.0f-%.0f\n", lightThreshold, tempThresholdLow, tempThresholdHigh);
  Serial.printf("Auto: P:%d L:%d R:%d F:%d B:%d\n", pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto);
  Serial.printf("Windmill:%d Servo:%d\n", windmillOn, servoAngle);
}
```

- Dòng 106–112: phím '3' in toàn bộ ngưỡng + trạng thái 5 cờ tự động + cối xay + góc servo. Chỉ đọc, không động vào thiết bị.

```cpp
void debugWebserver() {
  Serial.println(F("\n--- WEBSERVER ---"));
  Serial.printf("WiFi:%s  IP:%s\n", WiFi.status()==WL_CONNECTED?"OK":"FAIL", WiFi.localIP().toString().c_str());
  Serial.println(F("Routes: / /api /toggle /auto /rgb /breathe /on /off /windmill /feed /demo /servo /threshold"));
}
```

- Dòng 115–118: phím '4' in tình trạng WiFi + danh sách route (để bạn đối chiếu nhanh xem server đã đăng ký đủ chưa). `? :` chọn chuỗi "OK"/"FAIL".

```cpp
void debugWifi() {
  Serial.println(F("\n--- WIFI ---"));
  Serial.printf("SSID:%s  Status:%d\n", ssid, WiFi.status());
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("IP:%s  MAC:%s  RSSI:%d\n", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str(), WiFi.RSSI());
  } else {
    WiFi.reconnect();
  }
}
```

- Dòng 122–129: phím '5'. Nếu đang kết nối: in IP, địa chỉ MAC và cường độ sóng (RSSI, càng gần 0 càng mạnh). Nếu mất kết nối: gọi `WiFi.reconnect()` thử nối ngay.

```cpp
void debugOled() {
  Serial.println(F("\n--- OLED ---"));
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print(F("DEBUG OLED"));
  display.setCursor(0,16); display.print(F("Line 1"));
  display.setCursor(0,32); display.print(F("Line 2"));
  display.setCursor(0,48); display.print(F("Line 3"));
  display.display();
  Serial.println(F("OLED test pattern sent."));
}
```

- Dòng 133–144: phím '6' vẽ mẫu thử 4 dòng cách nhau 16 pixel. Nếu màn hình hiện đúng 4 dòng này nghĩa là OLED + dây I2C hoạt động tốt.

```cpp
void debugSettings() {
  Serial.println(F("\n--- SETTINGS ---"));
  Serial.printf("soilLow:%.0f soilHigh:%.0f\n", soilThresholdLow, soilThresholdHigh);
  Serial.printf("waterLow:%.0f lightThr:%d\n", waterThresholdLow, lightThreshold);
  Serial.printf("tempHigh:%.0f tempLow:%.0f\n", tempThresholdHigh, tempThresholdLow);
  Serial.printf("pump:%d light:%d rgb:%d fan:%d breathe:%d\n", pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto);
}
```

- Dòng 148–154: phím '7' in các giá trị đang được lưu flash (so sánh với web để kiểm tra lưu/đọc có đúng).

```cpp
void debugLogger() {
  Serial.println(F("\n--- LOGGER ---"));
  LOG_DEBUG("Debug message");
  LOG_INFO("Info message");
  LOG_WARN("Warning message");
  LOG_ERROR_CODE(999, "Error with code");
  LOG_FATAL("Fatal message");
}
```

- Dòng 157–164: phím '8' in thử đủ 5 mức log để kiểm tra định dạng. Mã 999 là mã giả định chỉ dùng cho test.

---

## PHẦN 5 — src/sensors.h và src/sensors.cpp (đọc + lọc cảm biến)

### 5.1. sensors.h

```cpp
#ifndef SENSORS_H
#define SENSORS_H

enum Weather { WEATHER_NORMAL, WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_RAIN };
```

- Dòng 5: 4 trạng thái thời tiết suy đoán từ ánh sáng + độ ẩm (đánh số 0, 1, 2, 3).

```cpp
void readAllSensors();
float readTemperature();
float readHumidity();
int   readLight();
int   readSoil();
int   readWater();
bool  readTouch();
bool  readPIR();
Weather weatherStatus();
```

- Dòng 7–15: "menu" các hàm đọc cảm biến: đọc từng cái và đọc tất cả, cộng thêm hàm đoán thời tiết.

### 5.2. sensors.cpp

```cpp
static float sTempBuf[SENSOR_FILTER_SIZE] = {0};
static float sHumBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sLightBuf[SENSOR_FILTER_SIZE] = {0};
static int   sSoilBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sWaterBuf[SENSOR_FILTER_SIZE] = {0};
static uint8_t sBufIdx = 0;
static bool sBufFilled = false;
```

- Dòng 6–10: 5 "sổ ghi vòng" (circular buffer) — mỗi cuốn giữ 5 giá trị gần nhất của 1 loại cảm biến.
- Dòng 11: vị trí đang ghi trong sổ (0…4). Ghi hết 4 thì quay lại 0 — ghi đè giá trị cũ nhất.
- Dòng 12: `sBufFilled` cho biết đã đủ 5 mẫu chưa (chưa đủ thì tính trung bình trên số mẫu đang có).

```cpp
static float avg(float* buf, uint8_t n) {
  float sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += buf[i];
  return sum / n;
}

static int avgInt(int* buf, uint8_t n) {
  long sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += buf[i];
  return (int)(sum / n);
}
```

- Dòng 14–18: hàm tính trung bình cộng cho mảng số thực. Cộng dồn rồi chia `n`.
- Dòng 20–24: tương tự cho mảng số nguyên; dùng biến `long` để cộng 5 số 4095 không bị tràn. `(int)(...)` đổi kết quả về số nguyên.

```cpp
float readTemperature() { return dht.readTemperature(); }
float readHumidity()    { return dht.readHumidity(); }
```

- Dòng 26–27: gọi thẳng thư viện DHT. Lưu ý: nếu cảm biến lỗi, 2 hàm này trả về `NaN` ("Not a Number" — không phải số) — phần xử lý nằm ở `readAllSensors`.

```cpp
static int normalizeAdc(int value, bool invert) {
  return invert ? (4095 - value) : value;
}
```

- Dòng 31–33: chuẩn hóa chiều cảm biến. ADC trả 0…4095; nếu cờ đảo = 1 thì lấy `4095 - value` để lật ngược (ví dụ module nước của bạn đọc ngược). Nhờ hàm này, **toàn bộ logic phía sau chỉ cần biết 1 quy ước duy nhất**.

```cpp
int   readLight()       { return normalizeAdc(analogRead(LDR_PIN), INVERT_LIGHT); }
int   readSoil()        { return normalizeAdc(analogRead(SOIL_PIN), INVERT_SOIL); }
int   readWater()       { return normalizeAdc(analogRead(WATER_PIN), INVERT_WATER); }
bool  readTouch()       { return digitalRead(TOUCH_PIN) == HIGH; }
bool  readPIR()         { return digitalRead(PIR_PIN) == HIGH; }
```

- Dòng 35–37: 3 cảm biến ADC (quang trở, đất, nước) qua `normalizeAdc`.
- Dòng 38–39: 2 cảm biến số (chạm, PIR) — chỉ có 2 trạng thái: có tín hiệu (HIGH) hoặc không.

```cpp
void readAllSensors() {
  float t = readTemperature();
  float h = readHumidity();

  bool tIsNan = isnan(t);
  bool hIsNan = isnan(h);
```

- Dòng 41–47: đọc thô nhiệt độ + độ ẩm, rồi **kiểm tra NaN trước khi thay thế** — vì sau khi thay thế thì không còn biết bản gốc lỗi hay không.

```cpp
  if (tIsNan) {
    t = sBufFilled ? sTempBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
    if (++sensorFailCount > 5 && sensorHealthy) {
      sensorHealthy = false;
      LOG_ERROR_CODE(501, "DHT11 sensor failing repeatedly");
    }
  }
  if (hIsNan) {
    h = sBufFilled ? sHumBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
  }
```

- Dòng 50–56: nếu nhiệt độ lỗi: lấy lại **giá trị tốt gần nhất** trong sổ (ô ghi trước đó = `sBufIdx - 1`, cộng thêm 5 rồi `% 5` để xử lý trường hợp đang ở ô 0). Nếu chưa có mẫu nào thì dùng 0. Đồng thời đếm lỗi: quá 5 lần liên tiếp → đánh dấu cảm biến "ốm" và ghi mã 501 **đúng 1 lần** (nhờ điều kiện `&& sensorHealthy`).
- Dòng 57–59: độ ẩm lỗi được xử lý giống hệt (không đếm riêng vì nhiệt độ và độ ẩm đến từ cùng 1 con chip DHT11).

```cpp
  if (!tIsNan && !hIsNan) {
    sensorFailCount = 0;
    sensorHealthy = true;
  }
```

- Dòng 62–65: đọc thành công **cả hai** thì xóa bộ đếm lỗi và đánh dấu khỏe lại.

```cpp
  int l = readLight();
  int s = readSoil();
  int w = readWater();

  sTempBuf[sBufIdx] = t;
  sHumBuf[sBufIdx]  = h;
  sLightBuf[sBufIdx] = l;
  sSoilBuf[sBufIdx]  = s;
  sWaterBuf[sBufIdx] = w;
  sBufIdx = (sBufIdx + 1) % SENSOR_FILTER_SIZE;
  if (sBufIdx == 0) sBufFilled = true;
```

- Dòng 67–69: đọc 3 cảm biến ADC.
- Dòng 72–76: ghi 5 giá trị vào 5 cuốn sổ tại cùng 1 vị trí.
- Dòng 77: dịch vị trí ghi lên 1; hết ô 4 thì quay về 0 (`% 5` = phép chia lấy dư).
- Dòng 78: vừa quay về 0 nghĩa là đã ghi tròn đủ 5 mẫu → đánh dấu sổ đầy.

```cpp
  uint8_t n = sBufFilled ? SENSOR_FILTER_SIZE : sBufIdx;
  if (n == 0) n = 1;

  gTemp  = avg(sTempBuf, n);
  gHum   = avg(sHumBuf, n);
  gLight = avgInt(sLightBuf, n);
  gSoil  = avgInt(sSoilBuf, n);
  gWater = avgInt(sWaterBuf, n);
  gPir   = readPIR();
  gTouch = readTouch();
}
```

- Dòng 80–81: số mẫu để tính trung bình: đủ 5 nếu sổ đầy, ngược lại bằng số mẫu hiện có. Nếu bằng 0 (lần đọc đầu tiên — nhưng thực tế không xảy ra vì `sBufIdx` vừa tăng) thì ép thành 1 để **không bao giờ chia cho 0**.
- Dòng 83–87: cập nhật 5 giá trị toàn cục `g…` bằng trung bình động → số liệu mượt, ít nhảy.
- Dòng 88–89: PIR và chạm không cần lọc (chúng là tín hiệu bật/tắt).

```cpp
Weather weatherStatus() {
  bool bright = gLight > (lightThreshold + 50);
  bool dark = gLight < (lightThreshold - 50);
  bool humid = gHum > 80.0f;
  if (bright) return humid ? WEATHER_CLOUDY : WEATHER_SUNNY;
  if (dark)   return humid ? WEATHER_RAIN : WEATHER_CLOUDY;
  return WEATHER_NORMAL;
}
```

- Dòng 93–100: "dự báo thời tiết" đơn giản theo luật:
  - Sáng hơn ngưỡng + 50: ẩm cao (>80%) → Âm u (nhiều mây), khô → Nắng.
  - Tối hơn ngưỡng − 50: ẩm cao → Mưa, khô → Âm u.
  - Ở giữa ±50: Bình thường (vùng trễ, tránh nhảy trạng thái liên tục).

---

## PHẦN 6 — src/actuators.h và src/actuators.cpp (điều khiển thiết bị)

### 6.1. actuators.h

```cpp
void setRelay(bool on);
void setFan(bool on);
void setBuzzer(bool on);
void setBlueLed(bool on);
void setBreatheLed(bool on);
void setLedPwm(uint8_t value);
void setRgb(uint8_t r, uint8_t g, uint8_t b);
void setRgbPlant();
void setRgbAuto(bool on);
void setPumpAuto(bool on);
void setFanAuto(bool on);
void setLightAuto(bool on);
void setBreatheAuto(bool on);
```

- Dòng 4–16: "menu" của mô-đun chấp hành: 7 hàm điều khiển thiết bị + 5 hàm bật/tắt chế độ tự động. Mọi mô-đun khác **chỉ được điều khiển thiết bị qua các hàm này**, không tự ghi chân — đó là lý do file này được gọi là "một cửa".

### 6.2. actuators.cpp

```cpp
#include "globals.h"
#include "logger.h"
#include "actuators.h"
```

- Dòng 1–3: nạp biến chung, logger và bản khai báo của chính mình.

```cpp
void setRelay(bool on) {
  if (relayState == on) return;
  relayState = on;
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(RELAY_PIN, level ? HIGH : LOW);
  LOG_INFO(on ? "Pump: ON" : "Pump: OFF");
}
```

- Dòng 10: **tính bất biến (idempotent)**: nếu trạng thái đã đúng ý muốn thì thoát ngay — không ghi lại chân, không in log trùng. Nhờ vậy các hàm này có thể được gọi hàng nghìn lần mỗi giây mà vô hại.
- Dòng 11: nhớ trạng thái mới.
- Dòng 12: tính mức điện cần ghi. Với relay thường: bật = HIGH. Với relay "kích mức thấp": bật = LOW — dấu `!` là phép đảo (true ↔ false).
- Dòng 13: ghi ra chân. `? :` đổi true/false thành HIGH/LOW.
- Dòng 14: ghi log.

```cpp
static bool sBuzzerOn = false;
```

- Dòng 19: trạng thái còi là biến **static nội bộ** — người ngoài không sửa được, chỉ `setBuzzer` đổi. Vì còi có 2 kiểu (chủ động/thụ động), cần nhớ trạng thái logic thay vì chỉ đọc chân.

```cpp
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
```

- Dòng 22–23: bất biến như trên.
- Dòng 24–27: nếu là còi thụ động → bật bằng `tone()` phát xung 2000 Hz, tắt bằng `noTone()`. Nếu là còi chủ động → chỉ cần cấp mức điện.
- Dòng 30: chỉ log lúc bật (tắt còi không cần log để đỡ rối).

```cpp
void setFan(bool on) {
  if (fanState == on) return;
  fanState = on;
  digitalWrite(FAN_PIN, on ? HIGH : LOW);
  LOG_INFO(on ? "Fan: ON" : "Fan: OFF");
}
```

- Dòng 33–38: quạt — cùng khuôn mẫu bất biến. Quạt nối qua relay nên không cần logic đảo riêng.

```cpp
void setBlueLed(bool on) {
  if (on) {
    if (blueLedState && !breatheLedState) return;
    blueLedState = true;
    breatheLedState = false;    // steady light wins over breathing
    breatheBrightness = 0;
    setLedPwm(255);
  } else {
    if (!blueLedState && !breatheLedState) return;
    blueLedState = false;
    breatheLedState = false;
    breatheBrightness = 0;
    setLedPwm(0);
  }
  LOG_INFO(on ? "Blue LED: ON" : "Blue LED: OFF");
}
```

- Dòng 43–58: LED xanh là thiết bị **dùng chung** giữa "sáng đều" và "đèn thở". Luật chia sẻ: *lệnh sáng đều luôn thắng*.
  - Dòng 45: nếu đèn đã sáng đều rồi (và không phải đang thở) thì thôi.
  - Dòng 47–48: đặt trạng thái sáng đều, **tắt đèn thở**, xóa độ sáng dở dang.
  - Dòng 49: PWM 255 = sáng hết cỡ.
  - Dòng 51: khi tắt: nếu cả 2 chế độ đều đang tắt thì thôi.
  - Dòng 54–55: tắt hẳn, PWM về 0.
- Nhờ luôn xử lý cả hai cờ ở đây, firmware **không bao giờ** rơi vào trạng thái "vừa sáng đều vừa thở" — bộ test kiểm tra bất biến này trong hàng chục nghìn bước fuzz.

```cpp
void setLedPwm(uint8_t value) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(BLUE_LED_PIN, value);
#else
  ledcWrite(LED_PWM_CHANNEL, value);
#endif
}
```

- Dòng 62–68: "một cửa" duy nhất nói chuyện với phần cứng PWM. Core 3.x ghi theo chân; core 2.x ghi theo kênh. Mọi nơi khác trong dự án gọi hàm này thay vì gọi `ledcWrite` trực tiếp — muốn đổi cách điều khiển chỉ sửa 1 chỗ.

```cpp
void setBreatheLed(bool on) {
  if (breatheLedState == on) return;
  breatheLedState = on;
  breatheBrightness = 0;
  breatheDirection = 1;
  if (on) {
    blueLedState = false;       // breathing takes over the blue LED
  } else {
    setLedPwm(0);
  }
  LOG_INFO(on ? "Breathe: ON" : "Breathe: OFF");
}
```

- Dòng 71: bất biến.
- Dòng 72–74: đặt trạng thái thở, độ sáng bắt đầu từ 0 và đi lên (thở vào trước).
- Dòng 76: bật thở thì tắt cờ sáng đều (đảm bảo loại trừ lẫn nhau).
- Dòng 78: tắt thở thì kéo PWM về 0 ngay (không để LED đứng ở độ sáng dở).
- Phần "vẽ" nhịp thở nằm ở `controlBreathe()` (Phần 7) — hàm này chỉ đổi trạng thái.

```cpp
void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  rgbR = r; rgbG = g; rgbB = b;
  for (int i = 0; i < RGB_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB set");
}
```

- Dòng 84: nhớ màu hiện tại vào biến chung.
- Dòng 85–87: vòng lặp `for` duyệt 4 bóng LED (0→3), đặt màu từng bóng trong "bộ đệm" của thư viện (chưa hiện ra).
- Dòng 88: `show()` mới thật sự đẩy màu ra dây — kỹ thuật "gom rồi phát" của dải NeoPixel, giúp đổi 4 bóng trong cùng 1 khoảnh khắc, không bị giật.
- Dòng 89: log chỉ hiện khi bật công tắc debug Rgb.

```cpp
void setRgbPlant() {
  rgbR = 255; rgbG = 0; rgbB = 255;
  for (int i = 0; i < RGB_COUNT; i++) {
    bool red = (i % 2 == 0);
    strip.setPixelColor(i, red ? strip.Color(255, 0, 0) : strip.Color(0, 0, 255));
  }
  strip.show();
  MOD_DEBUG(Rgb, "RGB plant light");
}
```

- Dòng 93–101: mẫu "đèn tăng trưởng" HOẠT ĐỘNG 9: bóng chẵn (0, 2) đỏ, bóng lẻ (1, 3) xanh dương → xen kẽ đỏ–xanh. `i % 2 == 0` là "i chia hết cho 2".
- Dòng 94: ghi màu đại diện (đỏ + xanh) vào biến chung để `controlRgb` biết "đang bật mẫu cây" (nó so `rgbR==255 && rgbG==0 && rgbB==255`).

```cpp
void setRgbAuto(bool on)  { rgbAuto = on; }
void setPumpAuto(bool on) { pumpAuto = on; }
void setFanAuto(bool on)  { fanAuto = on; }
void setLightAuto(bool on){ lightAuto = on; }
void setBreatheAuto(bool on) { breatheAuto = on; }
```

- Dòng 103–107: 5 hàm một dòng — chỉ ghi cờ. Viết hàm cho từng cờ tuy hơi dài dòng nhưng giúp mọi nơi gọi cùng một kiểu (`setPumpAuto(false)`) thay vì gán biến trực tiếp, sau này muốn thêm log/lưu flash chỉ sửa 1 chỗ.

---

## PHẦN 7 — src/control.h và src/control.cpp (bộ luật tự động)

### 7.1. control.h

```cpp
void controlPump();
void controlFan();
void controlLight();
void controlRgb();
void controlBreathe();
void controlTouch();
void controlAlerts();

void setWindmill(bool on);
void controlWindmill();
void startFeed();
void stopFeed();
void controlFeed();
bool feedRunning();
```

- Dòng 4–10: 7 hàm "luật" được gọi mỗi vòng lặp.
- Dòng 13–18: 6 hàm cho 2 cơ cấu dùng chung servo (cối xay gió + cửa máng ăn) + hàm hỏi trạng thái máng ăn.

### 7.2. control.cpp

```cpp
static bool sWaterAlarm = false;
static bool sMotionAlarm = false;
static bool sPirLitLed = false;   // true = the PIR alarm was the one that lit the LED
```

- Dòng 9: nhớ "còi đang kêu vì nước cạn" — dùng để chỉ ghi log **1 lần cho mỗi đợt** cạn nước, không spam mỗi giây.
- Dòng 10: nhớ "PIR đã báo có người".
- Dòng 11: nhớ **ai là người bật đèn** — nếu PIR bật thì PIR sẽ tắt; nếu đèn vốn đã sáng (do người dùng hoặc tự động) thì PIR không được phép tắt oan.

```cpp
void controlPump() {
  if (gWater < (int)waterThresholdLow) {
    if (relayState) {
      setRelay(false);
      LOG_ERROR_CODE(201, "Low water, pump OFF");
    }
    return;
  }

  if (!pumpAuto) return;

  if (soilThresholdHigh <= soilThresholdLow) return;

  if (gSoil < (int)soilThresholdLow && !relayState) {
    setRelay(true);
    LOG_INFO("Soil dry, pump ON [202]");
  } else if (gSoil > (int)soilThresholdHigh && relayState) {
    setRelay(false);
    LOG_INFO("Soil wet, pump OFF [203]");
  }
}
```

- Dòng 16: **kiểm tra an toàn trước mọi thứ**: nước dưới ngưỡng?
- Dòng 17–20: nếu bơm đang chạy thì tắt ngay + ghi mã 201. Dù bạn bật thủ công, luật này vẫn thắng.
- Dòng 21: thoát hàm — hết nước thì không bàn chuyện tưới.
- Dòng 24: không ở chế độ tự động thì chỉ áp dụng luật an toàn ở trên.
- Dòng 27: chốt chặn cuối: nếu ngưỡng cao ≤ ngưỡng thấp (cấu hình sai, ví dụ web nhập lộn) thì thoát, tránh bơm bật–tắt như điên.
- Dòng 30–32: đất khô hơn ngưỡng thấp và bơm đang tắt → bật bơm (mã 202).
- Dòng 33–35: đất đủ ẩm hơn ngưỡng cao và bơm đang chạy → tắt bơm (mã 203).
- Vì bật dùng ngưỡng thấp, tắt dùng ngưỡng cao (cách nhau 300 trong cấu hình mặc định), bơm sẽ không "rung" quanh một mốc — đó là **hysteresis**.

```cpp
void controlFan() {
  if (!fanAuto) return;
  if (tempThresholdHigh <= tempThresholdLow) return;
  if (gTemp > tempThresholdHigh && !fanState) {
    setFan(true);
  } else if (gTemp < tempThresholdLow && fanState) {
    setFan(false);
  }
}
```

- Dòng 41: không tự động thì thôi (quạt không có luật an toàn bắt buộc).
- Dòng 44: chặn ngưỡng đảo, chống rung relay.
- Dòng 46–50: trên ngưỡng cao → bật; dưới ngưỡng thấp → tắt; ở giữa → giữ nguyên.

```cpp
void controlLight() {
  if (!lightAuto || breatheLedState || gPir) return;
  if (gLight < (lightThreshold - 50) && !blueLedState) {
    setBlueLed(true);
  } else if (gLight > (lightThreshold + 50) && blueLedState) {
    setBlueLed(false);
  }
}
```

- Dòng 57: "nhường đường": nếu tự động đang tắt, hoặc đèn thở đang sở hữu LED, hoặc PIR đang sở hữu → luật này không được đụng vào đèn. Ba điều kiện này chính là cách tránh "giằng co" giữa các tính năng dùng chung 1 bóng LED.
- Dòng 58–62: tối hơn ngưỡng −50 → bật; sáng hơn ngưỡng +50 → tắt; khoảng giữa là vùng trễ.

```cpp
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
```

- Dòng 69: chỉ chạy khi rgbAuto.
- Dòng 70–71: tối / sáng theo cùng ngưỡng ±50 như đèn xanh.
- Dòng 72: `plantOn` = màu hiện tại đúng là mẫu cây (đỏ + xanh dương) không?
- Dòng 73: `rgbOff` = đang tắt hết không?
- Dòng 74–78: tối mà chưa bật mẫu cây → bật; sáng mà chưa tắt → tắt.
- Cách viết này gọi là **stateless** (không cần biến nhớ thêm): nó so với *màu thật đang hiển thị*, nên nếu người dùng vừa chọn màu thủ công rồi mới bật lại tự động, firmware vẫn hành xử đúng (trước đây dùng biến nhớ từng bị lỗi này).

```cpp
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
```

- Dòng 83–88: đèn thở đang tắt thì dọn dẹp phòng vệ: nếu còn độ sáng dở (do lỗi/bất thường) thì xóa về 0. Bình thường nhánh này không cần chạy, nhưng giữ lại để chắc chắn pin không kẹt mức sáng.
- Dòng 91–93: van nhịp thời gian — chưa đủ 30 ms thì thôi (đây chính là "non-blocking": hàm được gọi liên tục nhưng chỉ hành động theo nhịp).
- Dòng 95: tính độ sáng kế tiếp = hiện tại + hướng × 5. Ép sang `int16_t` (số nguyên 16 bit có dấu) để phép cộng có thể ra số âm rồi mới kiểm tra.
- Dòng 96–101: chạm trần 255 thì đổi hướng đi xuống; chạm sàn 0 thì đổi hướng đi lên — tạo hiệu ứng lên xuống đều đặn.
- Dòng 103–104: ghi giá trị mới và đẩy ra PWM.

```cpp
void controlTouch() {
  static bool sPrevTouch = false;
  if (gTouch && !sPrevTouch) {
    unsigned long now = millis();
    if (now - lastTouch >= TOUCH_DEBOUNCE) {
      if (lightAuto) {  // manual action disables auto to avoid fighting
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
```

- Dòng 109: nhớ lần trước tay có đang chạm không → phát hiện **cạnh lên** (từ không chạm → chạm), mỗi lần bấm chỉ tính 1 lần dù giữ tay.
- Dòng 110: chỉ xử lý tại cạnh lên.
- Dòng 112: chống dội: lần bấm trước chưa đủ 300 ms thì bỏ qua (nút cảm ứng hay nhiễu, một cú chạm có thể tạo nhiều xung).
- Dòng 113–116: hành động thủ công thì tắt chế độ tự động của đèn đèn để tránh "ông bật, bà tắt" — và lưu xuống flash.
- Dòng 117–119: đảo trạng thái LED (đang bật → tắt, đang tắt → bật), ghi mốc thời gian, ghi log.
- Dòng 122: lưu trạng thái chạm cho lần sau (đặt **ngoài** mọi `if` để luôn cập nhật).

```cpp
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
```

- Dòng 127: nước dưới ngưỡng → bật còi (mỗi vòng gọi `setBuzzer(true)` nhưng hàm này bất biến nên chỉ kêu 1 lần lệnh).
- Dòng 129–132: chỉ ghi log mã 302 **một lần** khi bắt đầu đợt cạn nước (nhờ cờ `sWaterAlarm`).
- Dòng 135–138: nước đã đủ trở lại → tắt còi, hạ cờ. Còi ưu tiên hơn PIR: đang cạn nước thì `return` luôn, không xét PIR.

```cpp
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
```

- Dòng 141–146 (phát hiện người): chỉ xử lý tại cạnh lên. `sPirLitLed = !blueLedState` ghi nhớ "đèn trước đó đang tắt nên chính PIR bật nó". Nếu đèn vốn đã sáng (thủ công/tự động/đang thở), PIR không nhận công và **không** đụng vào đèn.
- Dòng 147–154 (người đi khỏi): nếu PIR là người bật đèn thì:
  - Nếu người dùng có bật "đèn thở" (breatheAuto) → **bật lại đèn thở** (đây là bug đã được sửa: trước kia đèn thở bị PIR tắt vĩnh viễn).
  - Ngược lại → tắt đèn như cũ.
- Toàn bộ hàm dùng cờ "nhớ cạnh" nên log chỉ xuất hiện khi trạng thái **đổi**, không spam mỗi giây.

```cpp
static int sWindmillAngle = 0;
static int8_t sWindmillDir = 1;
static unsigned long sWindmillUpdate = 0;

void setWindmill(bool on) {
  if (windmillOn == on) return;
  windmillOn = on;
  if (on) {
    stopFeed();             // shared servo
  } else {
    servo.write(WINDMILL_MIN_ANGLE);
  }
  LOG_INFO(on ? "Windmill: ON" : "Windmill: OFF");
}
```

- Dòng 152–154: góc hiện tại, hướng quét (+1/-1), mốc thời gian lần quét trước.
- Dòng 157–158: bất biến.
- Dòng 159–160: bật cối xay → nhả máng ăn trước, vì **2 cơ cấu dùng chung 1 servo**, không thể đồng thời.
- Dòng 161–163: tắt → đưa servo về góc gốc.

```cpp
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
```

- Dòng 168–170: van nhịp 30 ms như đèn thở.
- Dòng 172: mỗi nhịp quét 6°.
- Dòng 173–174: chạm 2 đầu mút thì kẹp lại và đảo chiều — cối xay "quay" qua lại liên tục.
- Dòng 175: gửi góc ra servo.

```cpp
static bool sFeedActive = false;
static unsigned long sFeedStart = 0;

void startFeed() {
  setWindmill(false);       // shared servo
  sFeedActive = true;
  sFeedStart = millis();
  servo.write(FEED_OPEN_ANGLE);   // gate open
  LOG_INFO("Feed: gate open");
}

void stopFeed() {
  sFeedActive = false;
}

void controlFeed() {
  if (!sFeedActive) return;
  if (millis() - sFeedStart >= FEED_DURATION) {
    sFeedActive = false;
    servo.write(FEED_CLOSE_ANGLE);  // gate closed
    LOG_INFO("Feed: gate closed");
  }
}

bool feedRunning() { return sFeedActive; }
```

- Dòng 179–180: cờ "máng đang mở" + mốc thời gian mở.
- Dòng 183: nhả cối xay trước khi mở máng (servo dùng chung).
- Dòng 184–186: ghi mốc giờ, quay servo tới góc mở.
- Dòng 190–192: `stopFeed()` chỉ hạ cờ (dùng khi muốn hủy lập tức, ví dụ người dùng điều khiển servo thủ công). Hàm này **không** tự quay servo vì người gọi sẽ tự quyết định góc.
- Dòng 194–201: khi máng đang mở, đủ 2000 ms thì đóng lại + hạ cờ. Một lần cho ăn đúng 2 giây — đủ để thức ăn rơi xuống nhưng không tràn.
- Dòng 203: hàm hỏi trạng thái cho web/OLED.

---

## PHẦN 8 — src/demo.h và src/demo.cpp (trình diễn 15 bước)

### 8.1. demo.h

```cpp
bool demoRunning();
const char* demoStepName();
void startDemo();
void updateDemo();
```

- Dòng 4: đang trình diễn không?
- Dòng 5: tên bước hiện tại (để hiện lên OLED và `/api`).
- Dòng 6: bắt đầu trình diễn.
- Dòng 7: "tíc" một nhịp trình diễn (gọi mỗi vòng lặp khi đang chạy).

### 8.2. demo.cpp

```cpp
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
```

- Dòng 9–13: danh sách 13 "hành động" mà trình diễn có thể làm. Máy tính đánh số lần lượt từ 0.
- Dòng 15–20: "khuôn" mô tả 1 bước: hành động lúc **vào bước**, hành động lúc **hết bước**, thời lượng (ms), tên hiển thị. `struct` = gói nhiều giá trị vào 1 kiểu dữ liệu.

```cpp
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
```

- Dòng 22–38: "kịch bản" 15 bước, tổng cộng **30.700 ms**. Tên bước viết không dấu (màn hình OLED không có font tiếng Việt). Mỗi bước thực hiện đúng chức năng của 1 hoạt động trong sách hướng dẫn kit (HD2…HD12). Ví dụ:
  - HD3: bật đèn thở trong 4 giây rồi tắt.
  - HD7: mở bơm 2,5 giây (có kiểm tra nước) rồi tắt.
  - HD8: kêu còi 1 giây.
  - HD10: mở cửa máng ăn 3 giây, bước sau đóng lại.

```cpp
static const uint8_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);
```

- Dòng 40: tự tính số bước = tổng kích thước mảng chia kích thước 1 phần tử (15). Thêm/bớt bước không cần sửa con số này.

```cpp
static bool sRunning = false;
static uint8_t sStep = 0;
static unsigned long sStepStart = 0;
static unsigned long sSweepUpdate = 0;
```

- Dòng 42–45: trạng thái nội bộ: đang chạy?, đang ở bước số mấy, mốc thời gian bắt đầu bước, mốc quét servo gần nhất.

```cpp
bool demoRunning() { return sRunning; }

const char* demoStepName() {
  return sRunning ? STEPS[sStep].name : "";
}
```

- Dòng 47: hàm hỏi trạng thái.
- Dòng 49–51: trả tên bước nếu đang chạy, ngược lại trả chuỗi rỗng (an toàn vì `sStep` có thể vượt mảng khi đã dừng).

```cpp
static void demoAction(DemoAction a) {
  switch (a) {
    case DA_LED_ON:      setBlueLed(true); break;
    case DA_LED_OFF:     setBlueLed(false); break;
    case DA_BREATHE_ON:  setBreatheLed(true); break;
    case DA_BREATHE_OFF: setBreatheLed(false); break;
    case DA_PUMP_ON:
      if (gWater >= (int)waterThresholdLow) setRelay(true);
      else LOG_WARN("Demo: low water, skip pump");
      break;
    case DA_PUMP_OFF:    setRelay(false); break;
    case DA_BEEP:        setBuzzer(true); break;
    case DA_PLANT_ON:    setRgbPlant(); break;
    case DA_RGB_OFF:     setRgb(0, 0, 0); break;
    case DA_WINDMILL:    break;  // swept continuously in updateDemo()
    case DA_SERVO0:      servo.write(WINDMILL_MIN_ANGLE); stopFeed(); break;
    case DA_FEED:        startFeed(); break;
    default: break;
  }
}
```

- Dòng 53–72: "bảng tra" hành động → lệnh cụ thể. Đáng chú ý:
  - `DA_PUMP_ON` (dòng 59–61): chỉ bật bơm nếu nước đủ — trình diễn **cũng phải tuân luật an toàn**, và có nhánh bỏ qua có ghi cảnh báo.
  - `DA_WINDMILL` không làm gì ngay — cối xay được quét liên tục ở `updateDemo()`.
  - `DA_SERVO0`: về góc gốc + nhả máng.
  - Mỗi `case` kết thúc bằng `break` để không "rơi" xuống case dưới.

```cpp
static void demoLogStep() {
  Serial.print(F("[DEMO] "));
  Serial.println(STEPS[sStep].name);
}
```

- Dòng 74–77: in tên bước kèm tiền tố `[DEMO] ` — bộ test đếm đúng 15 dòng này để xác nhận đủ bước.

```cpp
void startDemo() {
  if (sRunning) return;
  setWindmill(false);
  stopFeed();
  sRunning = true;
  sStep = 0;
  sStepStart = millis();
  LOG_INFO("===== DEMO START =====");
  demoLogStep();
  demoAction(STEPS[sStep].in);
}
```

- Dòng 80: đang chạy rồi thì bấm thêm cũng không sao (không khởi động chồng).
- Dòng 81–82: nhả cối xay + máng ăn trước khi bắt đầu.
- Dòng 83–85: bật cờ chạy, về bước 0, ghi mốc giờ.
- Dòng 88–89: in tên bước đầu và thực hiện hành động "vào bước" của bước 0.

```cpp
void updateDemo() {
  if (!sRunning) return;
  unsigned long now = millis();
  unsigned long elapsed = now - sStepStart;

  if (STEPS[sStep].in == DA_WINDMILL && now - sSweepUpdate >= WINDMILL_INTERVAL) {
    sSweepUpdate = now;
    int span = WINDMILL_MAX_ANGLE - WINDMILL_MIN_ANGLE;
    int phase = (int)(elapsed % 2000);
    int angle = WINDMILL_MIN_ANGLE + ((phase < 1000) ? (phase * span) / 1000
                                                     : ((2000 - phase) * span) / 1000);
    servo.write(angle);
  }
```

- Dòng 92–94: chưa chạy thì thôi; tính đã trôi qua bao lâu kể từ đầu bước.
- Dòng 97: chỉ quét servo khi bước hiện tại là HD6 và đủ nhịp 30 ms.
- Dòng 99: biên độ quét (180° − 0° = 180).
- Dòng 100: "pha" trong chu kỳ 2000 ms (0…1999) — tạo sóng tam giác.
- Dòng 101–102: góc = nửa đầu chu kỳ thì tăng dần, nửa sau giảm dần → cánh quay đều 2 chiều, mượt hơn là nhảy cóc.

```cpp
  if (elapsed < STEPS[sStep].ms) return;

  demoAction(STEPS[sStep].out);
  if (STEPS[sStep].in == DA_BEEP) setBuzzer(false);

  sStep++;
  if (sStep >= STEP_COUNT) {
    sRunning = false;
    setBlueLed(false);
    setBreatheLed(false);
    setRelay(false);
    setBuzzer(false);
    setRgb(0, 0, 0);
    servo.write(WINDMILL_MIN_ANGLE);
    stopFeed();
    if (breatheAuto) setBreatheLed(true);   // restore opt-in breathing (activity 3)
    LOG_INFO("===== DEMO DONE =====");
    return;
  }

  sStepStart = now;
  demoLogStep();
  demoAction(STEPS[sStep].in);
}
```

- Dòng 106: chưa hết thời lượng bước → chờ tiếp.
- Dòng 108: thực hiện hành động "hết bước" (ví dụ tắt bơm sau HD7).
- Dòng 109: riêng bước còi thì tắt còi khi hết bước (vì `DA_BEEP` chỉ có hành động vào).
- Dòng 111: sang bước kế.
- Dòng 112–123 (hết kịch bản): tắt sạch mọi thiết bị về trạng thái an toàn, đưa servo về gốc, đóng máng. Sau đó **khôi phục đèn thở nếu người dùng đã bật** (bug đã sửa: trước kia đèn thở tắt luôn sau trình diễn). Ghi log DONE rồi thoát.
- Dòng 125–127: nếu còn bước: ghi mốc giờ mới, in tên bước, chạy hành động đầu bước.
- Toàn bộ hàm chỉ được gọi khi `demoRunning()`, và vòng lặp chính đã tạm dừng tự động hóa trong lúc demo — nhờ vậy không có tranh chấp thiết bị.

---

## PHẦN 9 — src/oled.h và src/oled.cpp (màn hình 128×64)

### 9.1. oled.h

```cpp
void updateOLED(float t, float h, int light, int soil, int water);
```

- Dòng 4: hàm vẽ nhận 5 giá trị cảm biến làm tham số — người gọi truyền số liệu vào, hàm không tự đọc cảm biến. Cách này giúp bộ test gọi hàm với số liệu tùy ý.

### 9.2. oled.cpp

```cpp
static const char* weatherText() {
  switch (weatherStatus()) {
    case WEATHER_SUNNY:  return "Nang";
    case WEATHER_CLOUDY: return "Am u";
    case WEATHER_RAIN:   return "Mua";
    default:             return "B.thuong";
  }
}
```

- Dòng 9–16: đổi mã thời tiết thành chữ tiếng Việt **không dấu** (font mặc định của SSD1306 không có dấu). "B.thuong" viết tắt cho vừa 8 ký tự.

```cpp
void updateOLED(float t, float h, int light, int soil, int water) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
```

- Dòng 19: xóa toàn bộ màn hình (xóa trong bộ đệm RAM, chưa hiện).
- Dòng 20: chữ màu trắng (màn đơn sắc chỉ có bật/tắt điểm ảnh).
- Dòng 21: cỡ chữ 1 = 6×8 pixel mỗi ký tự → mỗi dòng cao 8 pixel, tối đa **21 ký tự** một dòng (128 ÷ 6).

```cpp
  display.setCursor(0, 0);
  display.print(F("Smart Farm"));

  display.setCursor(0, 8);
  display.print(F("TT: "));
  display.print(weatherText());
```

- Dòng 24–25: dòng 0 — tiêu đề.
- Dòng 28–30: dòng 1 (y=8) — "TT: " + thời tiết.

```cpp
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(t, 1);
  display.print(F("C H:"));
  display.print(h, 0);
  display.print(F("%"));
```

- Dòng 33–38: dòng 2 — ví dụ `T:26.5C H:60%`. `print(t, 1)` in số thực 1 chữ số thập phân; `print(h, 0)` in độ ẩm không thập phân cho gọn.

```cpp
  display.setCursor(0, 24);
  display.print(F("Light: "));
  display.print(light);

  display.setCursor(0, 32);
  display.print(F("Soil: "));
  display.print(soil);

  display.setCursor(0, 40);
  display.print(F("H2O:"));
  display.print(water);
  display.print(F(" P:"));
  display.print(relayState ? F("ON") : F("OFF"));
```

- Dòng 41–48: dòng 3 (ánh sáng) và dòng 4 (độ ẩm đất).
- Dòng 51–55: dòng 5 — mực nước + trạng thái bơm (`P:ON` / `P:OFF`).

```cpp
  display.setCursor(0, 48);
  if (WiFi.status() == WL_CONNECTED) {
    display.print(F("IP:"));
    display.print(WiFi.localIP());
  } else {
    display.print(F("WiFi: ---"));
  }
```

- Dòng 58–64: dòng 6 — có mạng thì in IP (để biết gõ gì lên trình duyệt), mất mạng thì ghi `WiFi: ---`.

```cpp
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
```

- Dòng 67–76: dòng 7 — "dòng trạng thái" theo thứ tự ưu tiên: đang demo → tên bước; đang quay cối xay → ON; đang cho ăn → "Cho an..."; bình thường → "San sang".
- Dòng 78: `display()` mới thật sự đẩy bộ đệm ra màn hình — nhờ vẽ vào bộ đệm trước, cả khung hình thay đổi trong 1 nhịp, không bị xé hình.

---

## PHẦN 10 — src/settings.h và src/settings.cpp (lưu cài đặt)

### 10.1. settings.h

```cpp
void saveSettings();
void loadSettings();
```

- Dòng 4–5: 2 hàm duy nhất — ghi và đọc.

### 10.2. settings.cpp

```cpp
void saveSettings() {
  preferences.begin("smartfarm", false);
  preferences.putFloat("soilLow", soilThresholdLow);
  preferences.putFloat("soilHigh", soilThresholdHigh);
  preferences.putFloat("waterLow", waterThresholdLow);
  preferences.putInt("lightThr", lightThreshold);
  preferences.putFloat("tempHigh", tempThresholdHigh);
  preferences.putFloat("tempLow", tempThresholdLow);
  preferences.putBool("pumpAuto", pumpAuto);
  preferences.putBool("lightAuto", lightAuto);
  preferences.putBool("rgbAuto", rgbAuto);
  preferences.putBool("fanAuto", fanAuto);
  preferences.putBool("breatheAuto", breatheAuto);
  preferences.end();
  LOG_INFO("Settings saved to flash");
}
```

- Dòng 7: mở "ngăn kéo" tên `smartfarm` ở chế độ đọc-ghi (`false` = không phải chỉ đọc). Ngăn kéo nằm trong bộ nhớ flash — tắt điện không mất.
- Dòng 8–18: ghi 11 giá trị: 6 ngưỡng + 5 cờ tự động. Mỗi giá trị có 1 cái tên ngắn ≤15 ký tự (giới hạn của NVS). Chỉ lưu **cài đặt**, không lưu trạng thái bật/tắt thiết bị — vì khi mất điện, an toàn nhất là mọi thiết bị về trạng thái tắt.
- Dòng 19: đóng ngăn kéo để ghi thật sự được hoàn tất.
- Dòng 20: log xác nhận.

```cpp
void loadSettings() {
  preferences.begin("smartfarm", true);
  soilThresholdLow  = preferences.getFloat("soilLow", 400.0f);
  soilThresholdHigh = preferences.getFloat("soilHigh", 700.0f);
  waterThresholdLow = preferences.getFloat("waterLow", 200.0f);
  lightThreshold    = preferences.getInt("lightThr", 500);
  tempThresholdHigh = preferences.getFloat("tempHigh", 30.0f);
  tempThresholdLow  = preferences.getFloat("tempLow", 25.0f);
  pumpAuto          = preferences.getBool("pumpAuto", true);
  lightAuto         = preferences.getBool("lightAuto", true);
  rgbAuto           = preferences.getBool("rgbAuto", true);
  fanAuto           = preferences.getBool("fanAuto", true);
  breatheAuto       = preferences.getBool("breatheAuto", false);  // opt-in (activity 3)
  preferences.end();
  LOG_INFO("Settings loaded from flash");
}
```

- Dòng 24: mở ngăn kéo chế độ chỉ đọc (`true`).
- Dòng 25–35: đọc từng giá trị; **tham số thứ hai là giá trị mặc định** dùng khi chưa từng lưu (lần chạy đầu tiên sau khi nạp firmware) — luôn trùng với giá trị trong `config.cpp`.
- Dòng 35: đèn thở mặc định `false` — đây là tính năng "tùy chọn bật", không tự bật để tránh gây bất ngờ.
- Dòng 36–37: đóng ngăn kéo + log.

---

## PHẦN 11 — src/wifi.h và src/wifi.cpp (giữ WiFi sống)

### 11.1. wifi.h

```cpp
void manageWiFi();
```

- Dòng 4: 1 hàm duy nhất, được gọi mỗi vòng lặp.

### 11.2. wifi.cpp

```cpp
void manageWiFi() {
  static bool sWarned = false;
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (sWarned) {
      sWarned = false;
      LOG_INFO("WiFi reconnected");
    }
    lastWifiCheck = now;
    return;
  }
```

- Dòng 9: cờ "đã cảnh báo mất mạng cho đợt này chưa".
- Dòng 12–19: nếu đang kết nối tốt: vừa nối lại thành công (cờ đang bật) thì ghi log "WiFi reconnected" **một lần** rồi hạ cờ; sau đó liên tục dời mốc `lastWifiCheck` về hiện tại để "đồng hồ thử lại" luôn được nạp đầy khi mạng ổn.

```cpp
  if (now - lastWifiCheck < WIFI_CHECK_INTERVAL) return;

  if (!sWarned) {
    LOG_ERROR_CODE(401, "WiFi lost, reconnecting...");
    sWarned = true;
  }
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  lastWifiCheck = now;
}
```

- Dòng 21: mất mạng nhưng chưa đủ 10 giây kể từ lần thử trước → chờ (không thử liên tục làm nghẽn).
- Dòng 23–26: lần cảnh báo đầu tiên của đợt mất mạng: ghi mã 401 và bật cờ. Những vòng sau **không** ghi lại → Serial không bị spam "WiFi lost" 100 lần/giây.
- Dòng 27–28: ngắt kết nối cũ (dọn trạng thái rác) rồi thử nối lại.
- Dòng 29: nạp lại đồng hồ thử lại → nhịp thử đúng 10 giây một lần.
- Điểm hay: điều kiện chỉ là "không CONNECTED", nên mọi trạng thái bệnh (đang dò, thất bại, mất SSID…) đều được chữa bằng cùng một cách.

---

## PHẦN 12 — src/webserver.h và src/webserver.cpp (web điều khiển)

### 12.1. webserver.h

```cpp
void handleRoot();
void handleApi();
void handleToggle();
void handleAuto();
void handleRgb();
void handleBreatheLed();
void handleLedOn();
void handleLedOff();
void handleServo();
void handleWindmill();
void handleFeed();
void handleDemo();
void handleThreshold();
```

- Dòng 4–16: 13 hàm xử lý cho 13 route (route thứ 14 — favicon — nằm trong `SmartFarm.ino` vì nó quá ngắn).

### 12.2. Trang HTML nhúng (dòng 12–225)

```cpp
static const char ROOT_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";
```

- Dòng 12: cả trang web được lưu **trong flash** (`PROGMEM`) thay vì RAM. Cú pháp `R"rawliteral( ... )rawliteral"` là "chuỗi thô": mọi ký tự bên trong (kể cả dấu ngoặc kép, xuống dòng) đều là văn bản bình thường, không cần "escape" — rất tiện cho HTML/JS.
- Bố cục trang (đọc theo thứ tự trong file):
  - Dòng 14–65: `<head>` + CSS. CSS định hình giao diện: thẻ thống kê, nút bấm, thanh trượt, badge trạng thái, tự co giãn theo màn hình điện thoại (`@media`). Biến `--primary:#2e7d32` (xanh lá)…, `*{box-sizing:border-box}` để kích thước phần tử tính cả viền cho dễ căn.
  - Dòng 68–87: thẻ "Cảm biến" — 8 dòng hiển thị nhiệt độ, độ ẩm KK, ánh sáng, đất, nước, thời tiết, chuyển động, chạm. Mỗi giá trị có `id` (ví dụ `valTemp`) để JavaScript cập nhật.
  - Dòng 89–179: thẻ "Điều khiển" — các nút Bật/Tắt, Tự động/Thủ công cho bơm, đèn, quạt, RGB, đèn thở; thanh trượt servo; nút cối xay gió và cho ăn; hàng nút "chế độ tự động" tổng hợp; nút Trình diễn; thanh trạng thái 6 mục.
  - Mỗi nút gọi 1 hàm JavaScript qua `onclick=` — ví dụ `onclick="toggleDevice('pump')"`.
  - Dòng 182–222: `<script>` — "bộ não" phía trình duyệt.

### 12.3. JavaScript phía trình duyệt (dòng 183–221)

```javascript
async function fetchData(){
try{
const r=await fetch('/api');
const d=await r.json();
window.__sf=d;
```

- Dòng 183–187: hàm chính cập nhật giao diện. `async/await` = chờ lấy dữ liệu mà không đơ trang. Gọi `/api`, đổi JSON thành đối tượng `d`, cất vào `window.__sf` để các hàm khác dùng lại.

```javascript
document.getElementById('valTemp').textContent=d.temp?.toFixed(1)??'--';
```

- Dòng 188–196: đổ số liệu cảm biến vào các ô chữ. `?.` = "nếu có thì dùng" (tránh lỗi khi thiếu), `??` = "nếu rỗng/không có thì thay bằng". Riêng thời tiết và PIR/chạm được đổi mã số thành chữ tiếng Việt qua bảng tra `wt[...]` và `? :`.

```javascript
const p=document.getElementById('badgePump');p.textContent=d.relay?'Bật':'Tắt';p.className='badge '+(d.relay?'on':'off');
```

- Dòng 197–201: cập nhật các "badge" trạng thái (Bật/Tắt, Đang quay/Đang cho ăn/Tắt) — đổi cả chữ lẫn màu (`class on` = xanh, `off` = xám).
- Dòng 202–209: cập nhật thanh trạng thái tự động + tên bước demo đang chạy.

```javascript
}catch(e){console.error(e)}
}
```

- Dòng 210: nếu mạng lỗi thì ghi lỗi vào console trình duyệt (F12) — không làm treo giao diện.

```javascript
async function toggleDevice(id){await fetch('/toggle?id='+id);fetchData()}
async function ledSet(on){await fetch(on?'/on':'/off');fetchData()}
async function toggleWindmill(){const s=window.__sf||{};await fetch('/windmill?on='+(s.windmill?'false':'true'));fetchData()}
async function feedNow(){await fetch('/feed');fetchData()}
async function runDemo(){if(!confirm('Chạy trình diễn lần lượt các hoạt động (khoảng 40 giây)?'))return;await fetch('/demo');fetchData()}
async function setAutoMode(id,on){await fetch('/auto?id='+id+'&on='+on);fetchData()}
async function setRgb(r,g,b){await fetch('/rgb?r='+r+'&g='+g+'&b='+b);fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await fetch('/servo?angle='+v)}
```

- Dòng 212–219: mỗi hàm gửi 1 yêu cầu HTTP tới route tương ứng rồi gọi `fetchData()` cập nhật lại màn hình. Vài điểm đáng chú ý:
  - `toggleWindmill` đọc trạng thái hiện tại từ `window.__sf` để gửi `on=true/false` (vì route cần giá trị tường minh, không tự đảo).
  - `runDemo` hỏi `confirm()` trước khi chạy — tránh bấm nhầm.
  - `setServo` cập nhật nhãn góc ngay lập tức cho mượt.

```javascript
setInterval(fetchData,2000);
fetchData();
```

- Dòng 220: tự cập nhật mỗi 2 giây. Dòng 221: gọi ngay 1 lần lúc mở trang.
- Nhờ hàm `fetchData` chạy nền, mọi số liệu trên web luôn tươi mà người dùng không phải bấm "tải lại".

### 12.4. handleRoot — trang chủ (dòng 227–230)

```cpp
void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");
  server.send_P(200, "text/html", ROOT_HTML);
}
```

- Dòng 228: ghi log chi tiết nếu công tắc debug Webserver bật.
- Dòng 229: gửi trang HTML với mã **200 = OK**. `send_P` = gửi thẳng từ flash, không copy vào RAM — tiết kiệm bộ nhớ.

### 12.5. handleApi — dữ liệu JSON (dòng 233–265)

```cpp
void handleApi() {
  String json;
  json.reserve(520);
  json += "{";
```

- Dòng 234: log debug.
- Dòng 238–240: tạo chuỗi JSON. `reserve(520)` xin trước 520 ký tự bộ nhớ — tránh việc chuỗi phải "mọc" nhiều lần gây phân mảnh RAM.

```cpp
  json += "\"temp\":" + String(gTemp, 1) + ",";
  json += "\"hum\":" + String(gHum, 1) + ",";
  json += "\"light\":" + String(gLight) + ",";
  json += "\"soil\":" + String(gSoil) + ",";
  json += "\"water\":" + String(gWater) + ",";
  json += "\"weather\":" + String((int)weatherStatus()) + ",";
  json += "\"pir\":" + String(gPir ? "true" : "false") + ",";
  json += "\"touch\":" + String(gTouch ? "true" : "false") + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false") + ",";
  json += "\"fan\":" + String(fanState ? "true" : "false") + ",";
  json += "\"led\":" + String(blueLedState ? "true" : "false") + ",";
  json += "\"breathe\":" + String(breatheLedState ? "true" : "false") + ",";
  json += "\"windmill\":" + String(windmillOn ? "true" : "false") + ",";
  json += "\"feed\":" + String(feedRunning() ? "true" : "false") + ",";
  json += "\"demo\":" + String(demoRunning() ? "true" : "false") + ",";
  json += "\"demoStep\":\"" + String(demoRunning() ? demoStepName() : "") + "\",";
  json += "\"pumpAuto\":" + String(pumpAuto ? "true" : "false") + ",";
  json += "\"lightAuto\":" + String(lightAuto ? "true" : "false") + ",";
  json += "\"rgbAuto\":" + String(rgbAuto ? "true" : "false") + ",";
  json += "\"fanAuto\":" + String(fanAuto ? "true" : "false") + ",";
  json += "\"breatheAuto\":" + String(breatheAuto ? "true" : "false");
  json += "}";
```

- Dòng 242–247: 6 giá trị cảm biến — số thực in 1 chữ số thập phân (`String(gTemp, 1)`), thời tiết là mã 0–3.
- Dòng 248–260: 13 giá trị boolean, in đúng chữ `true`/`false` theo chuẩn JSON (không phải 1/0).
- Dòng 256: `demoStep` là chuỗi — khi không chạy demo thì trả chuỗi rỗng `""`.
- Dòng 261: đóng ngoặc nhọn.
- Lưu ý: dấu `\"` trong code nghĩa là "in ra ký tự ngoặc kép" (vì ngoặc kép không dấu huyền đã dùng để bao chuỗi C++).
- Dòng 264: gửi chuỗi với mã 200 và kiểu nội dung `application/json`. Trình duyệt sẽ tự hiểu đây là dữ liệu, không phải trang web.

```cpp
static uint8_t clampByte(long v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}
```

- Dòng 267–271: "kẹp" giá trị vào khoảng 0…255. Người dùng có thể gõ `r=999` hay `r=-5`; hàm này bảo đảm màu hợp lệ trước khi gửi xuống phần cứng.

### 12.6. handleToggle — nút Bật/Tắt (dòng 274–295)

```cpp
void handleToggle() {
  String id = server.arg("id");
  if (id == "pump") {
    setPumpAuto(false);
    setRelay(!relayState);
  } else if (id == "led") {
    setLightAuto(false);
    setBlueLed(!blueLedState);
  } else if (id == "fan") {
    setFanAuto(false);
    setFan(!fanState);
  } else if (id == "breathe") {
    bool want = !breatheLedState;
    setBreatheAuto(false);
    setBreatheLed(want);
  } else {
    server.send(400, "text/plain", "unknown id");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 275: đọc tham số `id` từ URL, ví dụ `/toggle?id=pump`.
- Dòng 276–279 (bơm): **tắt chế độ tự động trước**, rồi đảo trạng thái bơm. Quy tắc chung của cả dự án: điều khiển thủ công luôn tắt tự động tương ứng — để máy không "giằng co" với người.
- Dòng 280–284: đèn xanh tương tự (đảo + tắt lightAuto).
- Dòng 285–288: quạt.
- Dòng 289–292 (đèn thở): tính trạng thái mong muốn **trước khi** tắt tự động (vì hàm `setBreatheLed` đọc trạng thái hiện tại), rồi chuyển chế độ thủ công và áp dụng.
- Dòng 293–296: `id` lạ → trả mã **400 = yêu cầu sai**.
- Dòng 297–299: lưu cài đặt (cờ tự động thay đổi) rồi trả `200 OK` với chữ "ok".

### 12.7. handleAuto — bật/tắt tự động (dòng 298–314)

```cpp
void handleAuto() {
  String id = server.arg("id");
  bool on = server.arg("on") == "true";
  if (id == "pump") setPumpAuto(on);
  else if (id == "light") setLightAuto(on);
  else if (id == "rgb") setRgbAuto(on);
  else if (id == "fan") setFanAuto(on);
  else if (id == "breathe") {
    setBreatheAuto(on);
    setBreatheLed(on);
  } else {
    server.send(400, "text/plain", "unknown id");
    return;
  }
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 300: `on` chỉ là `true` khi chuỗi tham số **đúng chữ** "true" — mọi giá trị khác (kể cả "1") coi là false. Nhờ vậy hành vi rõ ràng, dễ test.
- Dòng 301–307: gán cờ theo `id`. Riêng "breathe": bật tự động thì **bật đèn thở luôn** (để người dùng thấy ngay), tắt thì tắt đèn.
- Dòng 308–311: `id` lạ → 400.
- Dòng 312–314: lưu flash + trả ok.

### 12.8. handleRgb — đổi màu LED RGB (dòng 317–325)

```cpp
void handleRgb() {
  uint8_t r = clampByte(server.arg("r").toInt());
  uint8_t g = clampByte(server.arg("g").toInt());
  uint8_t b = clampByte(server.arg("b").toInt());
  setRgbAuto(false);
  setRgb(r, g, b);
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 318–320: đọc 3 kênh màu, đổi chuỗi thành số (`toInt`), kẹp 0…255. Ô trống trả 0 → màu đen.
- Dòng 321–322: chọn màu thủ công thì tắt tự động RGB; sau đó đặt màu.

### 12.9. handleBreatheLed — nút riêng cho đèn thở (dòng 328–334)

```cpp
void handleBreatheLed() {
  bool want = !breatheLedState;
  setBreatheAuto(false);
  setBreatheLed(want);
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 329–331: lưu ý thứ tự: tính `want` trước, tắt tự động, rồi mới áp dụng. (Bug cũ "đèn thở không tắt được từ web" là do làm sai thứ tự — bộ test kiểm tra đúng chuỗi này.)

### 12.10. handleLedOn / handleLedOff — route `/on` `/off` (dòng 338–352)

```cpp
void handleLedOn() {
  MOD_DEBUG(Webserver, "GET /on");
  setLightAuto(false);
  setBlueLed(true);
  saveSettings();
  server.send(200, "text/plain", "LED ON");
}

void handleLedOff() {
  MOD_DEBUG(Webserver, "GET /off");
  setLightAuto(false);
  setBlueLed(false);
  saveSettings();
  server.send(200, "text/plain", "LED OFF");
}
```

- Đây là 2 route "kinh điển" của bài học (bật/tắt đèn từ xa). Cả hai tắt tự động đèn trước, rồi bật/tắt, lưu flash, trả lời bằng tiếng Anh ngắn `LED ON` / `LED OFF` (khớp tài liệu kit).

### 12.11. handleServo — điều khiển góc servo (dòng 355–364)

```cpp
void handleServo() {
  int angle = server.arg("angle").toInt();
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  setWindmill(false);   // manual servo control releases windmill and feed gate
  stopFeed();
  servoAngle = angle;
  servo.write(servoAngle);
  server.send(200, "text/plain", "ok");
}
```

- Dòng 356–358: đọc góc, kẹp vào 0…180 — servo chỉ quay được trong khoảng này, nhập ngoài khoảng sẽ kẹt bánh răng.
- Dòng 359–360: điều khiển tay thì **nhả cả 2 cơ cấu tự động** dùng chung servo (cối xay + máng ăn), nếu không servo sẽ bị 2 lệnh giành nhau.
- Dòng 361–362: nhớ góc vào biến chung rồi gửi ra servo.

### 12.12. handleWindmill — cối xay gió (dòng 367–371)

```cpp
void handleWindmill() {
  bool on = server.arg("on") == "true";
  setWindmill(on);
  server.send(200, "text/plain", on ? "windmill on" : "windmill off");
}
```

- Dòng 368–370: bật/tắt cối xay theo tham số tường minh `on=true|false`; trả lời theo trạng thái vừa áp dụng. Việc bật cối xay sẽ tự nhả máng ăn (đã xử lý trong `setWindmill`).

### 12.13. handleFeed — cho ăn (dòng 374–377)

```cpp
void handleFeed() {
  startFeed();
  server.send(200, "text/plain", "feeding");
}
```

- Dòng 375–376: mở cửa; việc đóng sau 2 giây do `controlFeed()` trong vòng lặp chính lo — bấm nút này không "khóa" web 2 giây.

### 12.14. handleDemo — trình diễn (dòng 380–384)

```cpp
void handleDemo() {
  MOD_DEBUG(Webserver, "GET /demo");
  startDemo();
  server.send(200, "text/plain", "demo started");
}
```

- Dòng 382–383: khởi động trình diễn và trả lời ngay. Trình diễn chạy nền trong vòng lặp chính, không làm treo web.

### 12.15. handleThreshold — chỉnh ngưỡng (dòng 387–409)

```cpp
void handleThreshold() {
  bool changed = false;
  if (server.hasArg("soilLow"))  { soilThresholdLow  = constrain(server.arg("soilLow").toFloat(),  0.0f, 4095.0f); changed = true; }
  if (server.hasArg("soilHigh")) { soilThresholdHigh = constrain(server.arg("soilHigh").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("waterLow")) { waterThresholdLow = constrain(server.arg("waterLow").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("light"))    { lightThreshold    = constrain(server.arg("light").toInt(),      0,    4095);   changed = true; }
  if (server.hasArg("tempHigh")) { tempThresholdHigh = constrain(server.arg("tempHigh").toFloat(), 0.0f, 60.0f);   changed = true; }
  if (server.hasArg("tempLow"))  { tempThresholdLow  = constrain(server.arg("tempLow").toFloat(),  0.0f, 60.0f);   changed = true; }
```

- Dòng 389–394: với **từng** tham số có mặt trong URL, đọc và kẹp vào khoảng hợp lệ:
  - Cảm biến ADC 0…4095; nhiệt độ 0…60 °C (DHT11 dải đo thực tế).
  - `hasArg` = "URL có tham số này không" → cho phép gửi 1, vài, hoặc cả 6 ngưỡng cùng lúc.
  - `toFloat` với chuỗi không phải số trả 0 (hành vi này được test khóa lại).

```cpp
  if (!changed) {
    server.send(400, "text/plain", "no params");
    return;
  }
  if (soilThresholdHigh <= soilThresholdLow) {
    soilThresholdHigh = constrain(soilThresholdLow + 50.0f, 0.0f, 4095.0f);
  }
  if (tempThresholdHigh <= tempThresholdLow) {
    tempThresholdHigh = tempThresholdLow + 1.0f;  // no upper clamp: keeps high > low at the 60 C edge
  }
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}
```

- Dòng 396–399: không có tham số nào → trả 400 "no params".
- Dòng 400–402: nếu người dùng đặt ngưỡng đất "cao" thấp hơn ngưỡng "thấp" → **tự sửa** thành thấp + 50 (kẹp tối đa 4095). Nhờ vậy web không thể tạo ra cấu hình làm bơm rung. (Ví dụ test: đặt soilLow=4095 thì soilHigh cũng thành 4095 — sát trần thì buộc phải bằng nhau, controlPump vẫn có chốt riêng chống rung.)
- Dòng 403–405: tương tự cho nhiệt độ nhưng cộng 1 và **không kẹp trần**: nếu tempLow=60 thì high thành 61 — vẫn giữ đúng bất biến high > low (test khóa hành vi này).
- Dòng 406–408: lưu flash, ghi log, trả ok.

---

## PHẦN 13 — Thư mục test/ (bộ kiểm thử 772.300 lượt)

### 13.1. Vì sao có bộ test này?

Không thể cắm kit ra cắm vào mỗi lần sửa code. Vì vậy dự án dùng kỹ thuật **mock** (giả lập
phần cứng): thay các thư viện ESP32 bằng phiên bản "đồ chơi" chạy trên máy tính, nhưng vẫn
biên dịch **code thật** (`SmartFarm.ino` + `src/*.cpp`). Nhờ vậy:

- Mỗi dòng code thật đều được chạy thử trên PC, nhanh gấp trăm lần kit.
- Thời gian "giả" điều khiển được từng mili-giây → test được cả trường hợp hiếm (đồng hồ tràn số).
- Chân GPIO, ADC, WiFi, web server… đều đọc/ghi được từ test → kiểm tra chính xác từng xung điện.

### 13.2. Cách chạy

```bash
./test/run_tests.sh --quick      # 1 bản build nhanh
./test/run_tests.sh --full       # 4 bản build (mặc định)
./test/run_tests.sh --coverage   # đo độ phủ dòng lệnh (gcov)
```

4 bản build của `--full` chạy CÙNG bộ test với cấu hình khác nhau, để chắc mọi nhánh
`#if` đều đúng:

| Bản build | Cấu hình | Kiểm tra điều gì |
|---|---|---|
| `core3` | Mặc định kit, core 3.x | Đường đi chính |
| `core2-invert` | Core 2.x + 3 cờ đảo cảm biến | Tương thích ngược + cảm biến đọc ngược chiều |
| `passive-lowrelay` | Còi thụ động + relay kích mức thấp | Nhánh `tone()` và nhánh đảo mức relay |
| `asan+ubsan` | Địa chỉ + hành vi không xác định | Bắt tràn bộ nhớ, dùng biến chưa khởi tạo… |

### 13.3. `test_main.cpp` gồm những gì?

- **T1–T17**: test đơn vị từng mô-đun — ví dụ T3 kiểm tra bơm bật khi đất khô, giữ nguyên khi đất trong khoảng trễ, tắt khi đất ẩm; T16 kiểm tra PIR không tắt oan đèn bật thủ công và đèn thở được khôi phục.
- **D1–D3**: soi đúng các giá trị **biên** — ví dụ `gWater` đúng bằng ngưỡng thì *được phép* chạy bơm (luật là `<` chứ không phải `<=`), hoặc van thời gian: đúng 29 ms chưa đổi nhịp, đúng 30 ms mới đổi.
- **I1–I11**: test tích hợp với `setup()` + `loop()` thật — boot đăng ký đủ 14 route, thứ tự ghi mức điện trước `pinMode` (chống glitch relay), mất WiFi tự nối lại, boot thiếu OLED/không có WiFi vẫn chạy, v.v.
- **D5–D14**: test chuyên sâu — hợp đồng JSON 21 khóa với bộ đọc JSON tự viết, đúng từng ký tự thân phản hồi, OLED từng dòng và giới hạn 21 ký tự, menu serial với byte rác, trình diễn đúng 15 bước/30.700 ms, chống spam WiFi đúng 10.000 ms, bền vững qua mất điện, **fuzz 90.000 bước ngẫu nhiên** với các bất biến an toàn (bơm không bao giờ chạy khô, chân luôn khớp trạng thái…), và tràn `millis()` cho mọi bộ đếm.

### 13.4. Vài bất biến (invariant) quan trọng được kiểm tra liên tục

1. `!(gWater < waterThresholdLow && relayState)` — không bao giờ bơm chạy khô.
2. Mức điện chân relay/quạt luôn khớp cờ trạng thái (kể cả khi đảo cực).
3. Cối xay gió và cửa máng ăn không bao giờ chạy đồng thời (chung server).
4. Đèn xanh "sáng đều" và "đèn thở" không bao giờ cùng bật.
5. Góc servo luôn trong 0…180; không có lượt ghi PWM/NeoPixel nào sai chỉ số.
6. Khi đèn tắt hẳn, PWM phải bằng 0 (không kẹt mức sáng cũ).

---

## PHẦN 14 — Bảng mã lỗi và thuật ngữ

### 14.1. Bảng mã lỗi (in ra dạng `[MA LOI: xxx]`)

| Mã | Ý nghĩa | Xử lý |
|---|---|---|
| 101 | Không tìm thấy màn hình OLED | Vẫn chạy tiếp; kiểm tra dây I2C, địa chỉ 0x3C |
| 102 | Hết 30 giây chưa kết nối WiFi | Tự thử lại mỗi 10 giây ở nền |
| 201 | Nước thấp, buộc tắt bơm | Châm nước; bơm tự được phép chạy lại |
| 202 | Đất khô, bật bơm (thông tin) | Bình thường |
| 203 | Đất đủ ẩm, tắt bơm (thông tin) | Bình thường |
| 301 | Phát hiện chuyển động (PIR) | Bật đèn theo HOẠT ĐỘNG 5 |
| 302 | Báo động nước cạn (còi) | Châm nước để tắt còi |
| 401 | Mất WiFi, đang nối lại | Chỉ ghi 1 lần mỗi đợt; tự nối lại |
| 501 | Cảm biến DHT11 lỗi liên tục (>5 lần) | Kiểm tra dây/chân 23; giá trị cũ được dùng tạm |

### 14.2. Bảng chân — bản đầy đủ kèm tên biến trong code

| Tên trong code | Chân | Ghi chú |
|---|---|---|
| `DHT_PIN` | 23 | DHT11 |
| `RGB_PIN` | 19 | NeoPixel 4 bóng |
| `SERVO_PIN` | 13 | Servo chung (cối xay + máng ăn) |
| `BUZZER_PIN` | 25 | Còi |
| `RELAY_PIN` | 26 | Relay bơm |
| `FAN_PIN` | 14 | Relay quạt |
| `BLUE_LED_PIN` | 27 | LED xanh (PWM) |
| `LED_PWM_CHANNEL` | kênh 15 | Chỉ dùng ở core 2.x |
| `TOUCH_PIN` | 39 | Cảm biến chạm |
| `PIR_PIN` | 33 | Cảm biến chuyển động |
| `LDR_PIN` | 34 | Quang trở |
| `SOIL_PIN` | 35 | Độ ẩm đất |
| `WATER_PIN` | 36 | Mực nước |
| `OLED_SDA/SCL` | 21 / 22 | I2C màn hình |

### 14.3. Câu hỏi thường gặp khi đọc code

- **"Vì sao dùng `now - last >= X` mà không phải `now >= last + X`?"** — Vì `millis()` sẽ tràn số
  về 0 sau ~49 ngày. Phép trừ của số không dấu luôn cho kết quả đúng kể cả khi tràn, còn phép
  cộng thì sai. Cả dự án theo đúng khuôn này, và bộ test D14 chứng minh bằng cách cho đồng hồ
  tràn giữa chừng.
- **"Vì sao mọi hàm bật/tắt đều kiểm tra `if (đã đúng trạng thái) return;`?"** — Vì chúng được
  gọi hàng nghìn lần mỗi giây. Không kiểm tra thì chân bị ghi liên tục, log đầy Serial, và các
  thiết bị cơ như servo/relay sẽ nhanh hỏng.
- **"Muốn đổi ngưỡng mặc định thì sửa ở đâu?"** — `src/config.cpp` (giá trị xuất xưởng) và
  `src/settings.cpp` (giá trị mặc định khi đọc flash) — sửa cả hai cho khớp. Ngưỡng người dùng
  đã lưu trên flash sẽ **không** bị ảnh hưởng; muốn về mặc định hãy chỉnh lại từ web.
- **"Kit nhà tôi đọc cảm biến ngược thì sao?"** — Đặt cờ `INVERT_LIGHT/SOIL/WATER` = 1 trong
  `src/config.h`, không phải sửa bất kỳ dòng logic nào.
- **"Relay/còi của tôi khác loại?"** — Đặt `RELAY_ACTIVE_LOW` hoặc `BUZZER_PASSIVE` trong
  `src/config.h` cho đúng module bạn có.

---

*Tài liệu này được viết kèm theo bộ test `test/`. Khi code thay đổi, hãy chạy
`./test/run_tests.sh --full` — nếu còn xanh nghĩa là tài liệu và code vẫn khớp hành vi.*
