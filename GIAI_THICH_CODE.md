# GIẢI THÍCH CODE TỪNG DÒNG — SMART FARM QE002 ACEBOTT

Tài liệu này dành cho **người chưa từng học lập trình** cũng đọc hiểu được. Mỗi đoạn code
đều được trích ra và giải thích ngay bên dưới: dòng đó làm gì, vì sao cần nó.

> Mẹo đọc: mở file code thật bên cạnh (ví dụ `SmartFarm.ino`) rồi đối chiếu số dòng.
> Code trong tài liệu được trích nguyên văn từ repo.

> **Lưu ý phiên bản:** tài liệu được viết trước đợt tái cấu trúc state gần nhất.
> Mọi biến toàn cục (ví dụ `relayState`, `soilThresholdLow`, `webToken`...) nay nằm
> trong một struct duy nhất `SystemState` và được truy cập qua `sys.relayState`,
> `sys.soilThresholdLow`... Logic, tên hàm và luồng hoạt động **không đổi**; khi đối
> chiếu với code hiện tại hãy thêm tiền tố `sys.` cho các biến toàn cục.
> Ngoài ra `webToken`/`otaPassword` không còn được ghi vào flash, và số dòng của
> `test/test_main.cpp` đã dịch chuyển do bổ sung test (T18, T19, I12, I13).

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

## PHẦN 1 — SmartFarm.ino (bộ não, 233 dòng)

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

### 1.8. setup() — chạy một lần khi bật nguồn (dòng 62–162)

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
  // WebServer chỉ giữ các header đã đăng ký (mặc định chỉ vài header trình duyệt);
  // phải khai báo để guardWeb() đọc được Origin/Referer/Sec-Fetch-Site/X-Auth-Token.
  static const char *watchedHeaders[] = {"Origin", "Referer", "Sec-Fetch-Site", "X-Auth-Token"};
  server.collectHeaders(watchedHeaders, 4);
  server.begin();
```

- Dòng 140–153: "đăng ký" 14 đường dẫn web. Mỗi dòng nghĩa là: khi có người mở địa chỉ này, hãy gọi hàm xử lý kia. Ví dụ mở `http://<IP>/?id=pump` của `/toggle` sẽ gọi `handleToggle()`.
- Dòng 154–155: chú thích giải thích lý do phải khai báo header trước: thư viện `WebServer` **chỉ giữ lại** những header đã được "điểm danh" (mặc định chỉ vài header quen thuộc của trình duyệt).
- Dòng 156–157: khai báo 4 header cần đọc (`Origin`, `Referer`, `Sec-Fetch-Site`, `X-Auth-Token`) rồi gọi `server.collectHeaders(...)` để thư viện lưu chúng. Nếu không khai báo, `guardWeb()` ở **PHẦN 12** không thấy `Origin`/`Referer`/`Sec-Fetch-Site` để chống CSRF và không thấy `X-Auth-Token` để kiểm token — vì vậy 4 header này phải được đăng ký ngay tại đây.
- Dòng 158: mở "quầy tiếp tân" — từ đây server mới bắt đầu nhận khách.

```cpp
  LOG_INFO("System ready");
  printDebugMenu();
}
```

- Dòng 160–161: ghi log "sẵn sàng" và in menu debug lần đầu.

### 1.9. loop() — chạy lặp mãi mãi (dòng 164–233)

```cpp
void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  manageWiFi();
```

- Dòng 165: "đút thức ăn" cho watchdog — bắt buộc ở đầu mỗi vòng.
- Dòng 166: xử lý mọi yêu cầu web đang chờ (nếu có).
- Dòng 167: kiểm tra WiFi, tự nối lại nếu rớt (chi tiết ở PHẦN 11).

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

- Dòng 170: có ký tự người dùng gõ trên Serial Monitor không?
- Dòng 171: đọc 1 ký tự.
- Dòng 172–173: nếu là ký tự xuống dòng (`\n`, `\r`) thì bỏ qua — vì Serial Monitor thường gửi kèm.
- Dòng 174: nếu **không** trong debug:
  - Dòng 175–176: gõ `9` → chạy trình diễn 15 bước.
  - Dòng 177–179: gõ `0`…`8` → vào debug và gọi đúng hàm của phím.
  - Dòng 180–183: gõ ký tự khác → báo sai cú pháp và in lại menu.
- Dòng 184–191: nếu **đang** debug: chỉ phím `0` mới có tác dụng (thoát); phím khác nhắc lại. Đây là "khóa an toàn" — trong lúc thử tay, bạn không vô tình chạy demo.

```cpp
  if (sDebugMode) return;
```

- Dòng 194: đang debug thì **dừng vòng lặp tại đây**. Nghĩa là toàn bộ tự động hóa (bơm, quạt, còi…) tạm nghỉ để bạn thử thiết bị bằng tay. Khi thoát debug, dòng đầu tiên của lần lặp kế tiếp sẽ khôi phục luật an toàn.

```cpp
  unsigned long now = millis();

  gTouch = readTouch();
  gPir = readPIR();
```

- Dòng 196: lấy đồng hồ hiện tại (mili-giây kể từ lúc bật). Dùng chung 1 giá trị cho cả vòng để logic nhất quán.
- Dòng 199–200: đọc 2 cảm biến số **mỗi vòng lặp** (không chờ 1 giây) vì nút chạm và PIR cần phản ứng tức thì.

```cpp
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    logSensors();   // one line every second at DEBUG level (activity 7)
    lastSensorRead = now;
  }
```

- Dòng 203: đã đủ 1000 ms kể từ lần đọc trước chưa? DHT11 yêu cầu cách nhau tối thiểu 1 giây nên các cảm biến được đọc theo nhịp riêng.
- Dòng 204: đọc tất cả cảm biến + lọc nhiễu + cập nhật các biến `g…`.
- Dòng 205: in 1 dòng tóm tắt cảm biến (chỉ hiện khi mức log là DEBUG).
- Dòng 206: ghi lại mốc thời gian vừa đọc.

```cpp
  if (gWater < (int)waterThresholdLow && relayState) setRelay(false);
```

- Dòng 210: **luật an toàn số 1 của cả firmware**: nếu mực nước thấp hơn ngưỡng mà bơm đang chạy thì tắt bơm ngay lập tức. Dòng này nằm ở đây (ngoài mọi nhánh) nên nó có hiệu lực **trong mọi chế độ**: tự động, thủ công, trình diễn, debug… Bơm chạy khô sẽ cháy.

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

- Dòng 213: nếu trình diễn đang chạy:
  - Dòng 214: chạy bước tiếp theo của trình diễn (khi nào hết giờ bước thì chuyển bước).
  - Dòng 215: vẫn cập nhật hiệu ứng "đèn thở" để bước HD3 nhấp nháy mượt.
- Dòng 216–226: ngược lại, chạy lần lượt 9 bộ luật tự động. Thứ tự có ý nghĩa:
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

- Dòng 229: cứ 2000 ms vẽ lại màn hình một lần (vẽ thường xuyên hơn sẽ nhấp nháy và tốn thời gian).
- Dòng 230: vẽ 8 dòng thông tin (thời tiết, nhiệt độ, độ sáng, đất, nước, IP, trạng thái).
- Dòng 231: ghi mốc thời gian vừa vẽ.

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

#ifndef WEB_TOKEN
#define WEB_TOKEN ""
#endif

#endif
```

- Dòng 82–93: 4 góc servo để hiệu chuẩn cơ khí: cối xay gió quét từ 0° đến 180°, cửa máng ăn mở 90° / đóng 0°. Tùy cách bạn lắp cánh tay servo, có thể phải chỉnh để không bị kẹt bánh răng.
- Dòng 95–102: khối bảo vệ web — `WEB_TOKEN`. Mặc định để rỗng `""` nghĩa là **KHÔNG bật token**: web mở như cũ. Nếu bạn đặt một chuỗi bất kỳ (ví dụ `"mat-khau-cua-toi"`), mọi lệnh đổi trạng thái (bật/tắt, chỉnh ngưỡng…) đều phải kèm token qua header `X-Auth-Token` hoặc tham số `?token=...`; trang chủ đọc token trên URL rồi tự gắn vào các lệnh. Cách kiểm tra token nằm ở `guardWeb()` (PHẦN 12).
- Dòng 104: kết thúc include guard.

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

```cpp
#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: xoá cửa sổ lọc trung bình về trạng thái vừa cấp điện.
void testResetSensorStatics() {
  for (uint8_t i = 0; i < SENSOR_FILTER_SIZE; i++) {
    sTempBuf[i] = sHumBuf[i] = 0;
    sLightBuf[i] = sSoilBuf[i] = sWaterBuf[i] = 0;
  }
  sBufIdx = 0;
  sBufFilled = false;
}
#endif
```

- Dòng 102–112: hook chỉ dành cho bộ test — chỉ được biên dịch khi build kèm `-DSMARTFARM_TEST_HOOKS`. Hàm `testResetSensorStatics()` xóa cả 5 cửa sổ lọc trung bình (`sTempBuf`, `sHumBuf`, `sLightBuf`, `sSoilBuf`, `sWaterBuf`) rồi trả `sBufIdx`/`sBufFilled` về mặc định, để `resetState()` của bộ test dọn sạch trạng thái giữa các test. Firmware nạp cho kit thật không chứa đoạn này.

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

```cpp
#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test trên máy tính: đưa biến static về trạng thái vừa cấp điện
// để các test không phụ thuộc vào thứ tự chạy.
void testResetActuatorStatics() { sBuzzerOn = false; }
#endif
```

- Dòng 109–113: hook chỉ dành cho bộ test — chỉ được biên dịch khi build kèm `-DSMARTFARM_TEST_HOOKS`. Hàm `testResetActuatorStatics()` trả biến static `sBuzzerOn` (trạng thái còi) về mặc định để `resetState()` của bộ test dọn sạch trạng thái giữa các test, không phụ thuộc thứ tự chạy. Bản firmware nạp cho kit thật không chứa đoạn này.

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
static bool sPrevTouch = false;   // trạng thái chạm lần trước (chống dội)
```

- Dòng 9: nhớ "còi đang kêu vì nước cạn" — dùng để chỉ ghi log **1 lần cho mỗi đợt** cạn nước, không spam mỗi giây.
- Dòng 10: nhớ "PIR đã báo có người".
- Dòng 11: nhớ **ai là người bật đèn** — nếu PIR bật thì PIR sẽ tắt; nếu đèn vốn đã sáng (do người dùng hoặc tự động) thì PIR không được phép tắt oan.
- Dòng 12: `sPrevTouch` — trạng thái chạm lần trước, dùng để phát hiện **cạnh lên** (từ không chạm → chạm) trong `controlTouch()`. Đây là biến static phạm vi file (khai báo ở đầu file thay vì bên trong hàm), nhờ vậy hook test ở cuối file mới reset được nó.

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

- Dòng 18: **kiểm tra an toàn trước mọi thứ**: nước dưới ngưỡng?
- Dòng 19–22: nếu bơm đang chạy thì tắt ngay + ghi mã 201. Dù bạn bật thủ công, luật này vẫn thắng.
- Dòng 23: thoát hàm — hết nước thì không bàn chuyện tưới.
- Dòng 26: không ở chế độ tự động thì chỉ áp dụng luật an toàn ở trên.
- Dòng 29: chốt chặn cuối: nếu ngưỡng cao ≤ ngưỡng thấp (cấu hình sai, ví dụ web nhập lộn) thì thoát, tránh bơm bật–tắt như điên.
- Dòng 32–34: đất khô hơn ngưỡng thấp và bơm đang tắt → bật bơm (mã 202).
- Dòng 35–37: đất đủ ẩm hơn ngưỡng cao và bơm đang chạy → tắt bơm (mã 203).
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

- Dòng 43: không tự động thì thôi (quạt không có luật an toàn bắt buộc).
- Dòng 46: chặn ngưỡng đảo, chống rung relay.
- Dòng 48–52: trên ngưỡng cao → bật; dưới ngưỡng thấp → tắt; ở giữa → giữ nguyên.

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

- Dòng 59: "nhường đường": nếu tự động đang tắt, hoặc đèn thở đang sở hữu LED, hoặc PIR đang sở hữu → luật này không được đụng vào đèn. Ba điều kiện này chính là cách tránh "giằng co" giữa các tính năng dùng chung 1 bóng LED.
- Dòng 60–64: tối hơn ngưỡng −50 → bật; sáng hơn ngưỡng +50 → tắt; khoảng giữa là vùng trễ.

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

- Dòng 71: chỉ chạy khi rgbAuto.
- Dòng 72–73: tối / sáng theo cùng ngưỡng ±50 như đèn xanh.
- Dòng 74: `plantOn` = màu hiện tại đúng là mẫu cây (đỏ + xanh dương) không?
- Dòng 75: `rgbOff` = đang tắt hết không?
- Dòng 76–80: tối mà chưa bật mẫu cây → bật; sáng mà chưa tắt → tắt.
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

- Dòng 85–90: đèn thở đang tắt thì dọn dẹp phòng vệ: nếu còn độ sáng dở (do lỗi/bất thường) thì xóa về 0. Bình thường nhánh này không cần chạy, nhưng giữ lại để chắc chắn pin không kẹt mức sáng.
- Dòng 93–95: van nhịp thời gian — chưa đủ 30 ms thì thôi (đây chính là "non-blocking": hàm được gọi liên tục nhưng chỉ hành động theo nhịp).
- Dòng 97: tính độ sáng kế tiếp = hiện tại + hướng × 5. Ép sang `int16_t` (số nguyên 16 bit có dấu) để phép cộng có thể ra số âm rồi mới kiểm tra.
- Dòng 98–103: chạm trần 255 thì đổi hướng đi xuống; chạm sàn 0 thì đổi hướng đi lên — tạo hiệu ứng lên xuống đều đặn.
- Dòng 105–106: ghi giá trị mới và đẩy ra PWM.

```cpp
void controlTouch() {
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

- Dòng 111: chỉ xử lý tại **cạnh lên** (`gTouch && !sPrevTouch`) — nhờ biến `sPrevTouch` ở dòng 12, mỗi lần bấm chỉ tính 1 lần dù giữ tay.
- Dòng 113: chống dội: lần bấm trước chưa đủ 300 ms thì bỏ qua (nút cảm ứng hay nhiễu, một cú chạm có thể tạo nhiều xung).
- Dòng 114–117: hành động thủ công thì tắt chế độ tự động của đèn đèn để tránh "ông bật, bà tắt" — và lưu xuống flash.
- Dòng 118–120: đảo trạng thái LED (đang bật → tắt, đang tắt → bật), ghi mốc thời gian, ghi log.
- Dòng 123: lưu trạng thái chạm cho lần sau (đặt **ngoài** mọi `if` để luôn cập nhật).

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

- Dòng 128: nước dưới ngưỡng → bật còi (mỗi vòng gọi `setBuzzer(true)` nhưng hàm này bất biến nên chỉ kêu 1 lần lệnh).
- Dòng 130–132: chỉ ghi log mã 302 **một lần** khi bắt đầu đợt cạn nước (nhờ cờ `sWaterAlarm`).
- Dòng 136–139: nước đã đủ trở lại → tắt còi, hạ cờ. Còi ưu tiên hơn PIR: đang cạn nước thì `return` luôn, không xét PIR.

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

- Dòng 145–149 (phát hiện người): chỉ xử lý tại cạnh lên. `sPirLitLed = !blueLedState` ghi nhớ "đèn trước đó đang tắt nên chính PIR bật nó". Nếu đèn vốn đã sáng (thủ công/tự động/đang thở), PIR không nhận công và **không** đụng vào đèn.
- Dòng 150–156 (người đi khỏi): nếu PIR là người bật đèn thì:
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

- Dòng 161–163: góc hiện tại, hướng quét (+1/-1), mốc thời gian lần quét trước.
- Dòng 166–167: bất biến.
- Dòng 168–169: bật cối xay → nhả máng ăn trước, vì **2 cơ cấu dùng chung 1 servo**, không thể đồng thời.
- Dòng 170–172: tắt → đưa servo về góc gốc.

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

- Dòng 177–179: van nhịp 30 ms như đèn thở.
- Dòng 181: mỗi nhịp quét 6°.
- Dòng 182–183: chạm 2 đầu mút thì kẹp lại và đảo chiều — cối xay "quay" qua lại liên tục.
- Dòng 184: gửi góc ra servo.

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

- Dòng 188–189: cờ "máng đang mở" + mốc thời gian mở.
- Dòng 192: nhả cối xay trước khi mở máng (servo dùng chung).
- Dòng 193–195: ghi mốc giờ, quay servo tới góc mở.
- Dòng 199–201: `stopFeed()` chỉ hạ cờ (dùng khi muốn hủy lập tức, ví dụ người dùng điều khiển servo thủ công). Hàm này **không** tự quay servo vì người gọi sẽ tự quyết định góc.
- Dòng 203–210: khi máng đang mở, đủ 2000 ms thì đóng lại + hạ cờ. Một lần cho ăn đúng 2 giây — đủ để thức ăn rơi xuống nhưng không tràn.
- Dòng 212: hàm hỏi trạng thái cho web/OLED.

```cpp
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
```

- Dòng 214–228: hook chỉ dành cho bộ test — chỉ được biên dịch khi build kèm `-DSMARTFARM_TEST_HOOKS`. Hàm `testResetControlStatics()` trả toàn bộ cờ static về mặc định: 3 cờ cảnh báo (`sWaterAlarm`, `sMotionAlarm`, `sPirLitLed`), `sPrevTouch`, 3 biến cối xay gió và 2 biến máng ăn. Nhờ vậy `resetState()` của bộ test dọn sạch trạng thái giữa các test — kể cả `sPrevTouch`, trước đây nằm bên trong `controlTouch()` nên không dọn được.

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
- Dòng 87–88: in tên bước đầu và thực hiện hành động "vào bước" của bước 0.

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
- Dòng 126–128: nếu còn bước: ghi mốc giờ mới, in tên bước, chạy hành động đầu bước.
- Toàn bộ hàm chỉ được gọi khi `demoRunning()`, và vòng lặp chính đã tạm dừng tự động hóa trong lúc demo — nhờ vậy không có tranh chấp thiết bị.

```cpp
#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: đưa trình diễn về trạng thái vừa cấp điện.
void testResetDemoStatics() {
  sRunning = false;
  sStep = 0;
  sStepStart = 0;
  sSweepUpdate = 0;
}
#endif
```

- Dòng 131–139: hook chỉ dành cho bộ test — chỉ được biên dịch khi build kèm `-DSMARTFARM_TEST_HOOKS`. Hàm `testResetDemoStatics()` đưa 4 biến static của trình diễn (`sRunning`, `sStep`, `sStepStart`, `sSweepUpdate`) về mặc định, để `resetState()` của bộ test dọn sạch trạng thái giữa các test. Firmware nạp cho kit thật không chứa đoạn này.

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
  } else if (sys.windmillOn) {
    display.print(F("Coi xay: ON"));
  } else if (feedRunning()) {
    display.print(F("Cho an..."));
  } else {
    display.print(F("San sang"));
  }
}
```

- Dòng 67–76: dòng 7 — "dòng trạng thái" theo thứ tự ưu tiên: đang demo → tên bước; đang quay cối xay → ON; đang cho ăn → "Cho an..."; bình thường → "San sang".

### 9.3. Đẩy I2C kiểu "delta" — chỉ gửi vùng pixel thực sự đổi

Sau khi vẽ xong vào bộ đệm RAM, firmware **không** gọi `display.display()` (hàm này luôn gửi đủ 1024 byte ra I2C ≈ 23 ms ở 400 kHz). Thay vào đó nó tự so sánh bộ đệm với khung trước và chỉ gửi phần thay đổi:

```cpp
static uint8_t sPrevFrame[SCREEN_WIDTH * SCREEN_HEIGHT / 8];
static bool sPrevFrameValid = false;
```

- `sPrevFrame`: bản sao khung hình đã hiển thị lần trước (1024 byte, đúng bằng kích thước bộ đệm SSD1306).
- `sPrevFrameValid = false` lúc đầu → lần vẽ đầu tiên gửi trọn màn hình (`oledWritePages`) bằng **một** lệnh địa chỉ + một luồng dữ liệu, rẻ nhất.

Với các lần vẽ sau, firmware quét từng page (page = 8 hàng pixel, tức đúng 1 dòng text với font 6×8) và tìm **dải cột đầu..cuối** bị đổi:

```cpp
for (uint8_t p = 0; p < pages; p++) {
  const uint8_t* cur  = frame + (size_t)p * SCREEN_WIDTH;
  const uint8_t* prev = sPrevFrame + (size_t)p * SCREEN_WIDTH;
  uint8_t c0 = 0xFF, c1 = 0;
  for (uint8_t c = 0; c < SCREEN_WIDTH; c++) {
    if (cur[c] != prev[c]) { if (c0 == 0xFF) c0 = c; c1 = c; }
  }
  if (c0 == 0xFF) continue;          // page này không đổi -> không đụng I2C
  oledWriteCols(cur, p, c0, c1);
}
```

- Quét **byte** trong page chứ không chỉ so sánh cả page: với font 6×8, mỗi ký tự chiếm đúng 6 cột, nên đổi 1 ký tự chỉ làm đổi 6 byte thay vì 128 byte của cả page.
- `oledWriteCols()` đặt địa chỉ rồi gói dữ liệu:

```cpp
Wire.write((uint8_t)0x21);   // set column address -> c0, c1
Wire.write((uint8_t)0x22);   // set page address  -> page, page (một page duy nhất)
...
oledWriteData(pageData + c0, (size_t)(c1 - c0 + 1));
```

- SSD1306 đang ở chế độ **địa chỉ ngang** (lệnh `0x20 0x00`, thư viện Adafruit_SSD1306 đặt trong `begin()`): con trỏ cột tự tăng khi ghi byte và quay về cột đầu khi chạm cột cuối. Vì ta đặt page là **một page duy nhất** (`page, page`) và ghi **đúng** `c1 - c0 + 1` byte, con trỏ không bao giờ vượt cột cuối và không nhảy sang page kế tiếp — nên gói dữ liệu rơi đúng vào vùng cần sửa.
- `oledWriteData()` chia gói theo 127 byte + 1 byte điều khiển `0x40` (D/C# = 1) vì buffer I2C mặc định của `Wire` chỉ chứa 128 byte.

**Hiệu quả đo được** (xem test `testOledDelta` — bộ đệm giả của mock mô phỏng đúng bố cục 6 cột/ký tự của font thật):

| Tình huống | Cách cũ (gửi cả dải page) | Cách mới (gửi đúng dải cột) |
|---|---|---|
| Vẽ lần đầu | 1024 byte | 1024 byte (không đổi) |
| Đổi 1 ký tự ("H2O:900"→"H2O:500") | 128 byte | **6 byte** (−95%) |
| Đổi 2 dòng số liệu | 384 byte | **42 byte** (−89%) |

Nhờ vậy thời gian chặn `loop()` mỗi lần cập nhật màn hình giảm mạnh (mỗi byte I2C ở 400 kHz tốn ~22 µs), web server phản hồi nhanh hơn.

`oledForceRedraw()` đặt `sPrevFrameValid = false` — dùng khi code khác vẽ trực tiếp lên màn hình (màn hình test của `debugOled()`) để bộ đệm so sánh delta không tưởng nhầm "không đổi" rồi bỏ qua.

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
static bool sWarned = false;   // đã ghi log mất WiFi cho đợt này chưa

void manageWiFi() {
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

- Dòng 8: cờ "đã cảnh báo mất mạng cho đợt này chưa" — nay là biến static phạm vi file (khai báo ở đầu file thay vì bên trong `manageWiFi()`), nhờ vậy hook test ở cuối file mới reset được nó.
- Dòng 13–19: nếu đang kết nối tốt: vừa nối lại thành công (cờ đang bật) thì ghi log "WiFi reconnected" **một lần** rồi hạ cờ; sau đó liên tục dời mốc `lastWifiCheck` về hiện tại để "đồng hồ thử lại" luôn được nạp đầy khi mạng ổn.

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

- Dòng 22: mất mạng nhưng chưa đủ 10 giây kể từ lần thử trước → chờ (không thử liên tục làm nghẽn).
- Dòng 24–27: lần cảnh báo đầu tiên của đợt mất mạng: ghi mã 401 và bật cờ. Những vòng sau **không** ghi lại → Serial không bị spam "WiFi lost" 100 lần/giây.
- Dòng 28–29: ngắt kết nối cũ (dọn trạng thái rác) rồi thử nối lại.
- Dòng 30: nạp lại đồng hồ thử lại → nhịp thử đúng 10 giây một lần.
- Điểm hay: điều kiện chỉ là "không CONNECTED", nên mọi trạng thái bệnh (đang dò, thất bại, mất SSID…) đều được chữa bằng cùng một cách.

```cpp
#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: quên trạng thái "đã cảnh báo mất WiFi".
void testResetWifiStatics() { sWarned = false; }
#endif
```

- Dòng 33–36: hook chỉ dành cho bộ test — chỉ được biên dịch khi build kèm `-DSMARTFARM_TEST_HOOKS`. Hàm `testResetWifiStatics()` xóa cờ `sWarned` để `resetState()` của bộ test dọn sạch trạng thái giữa các test, tránh việc test sau tưởng "đã cảnh báo mất WiFi" từ test trước. Firmware nạp cho kit thật không chứa đoạn này.

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

### 12.2. Trang HTML nhúng (dòng 12–228)

```cpp
static const char ROOT_HTML[] PROGMEM = R"rawliteral(
...
)rawliteral";
```

- Dòng 12: cả trang web được lưu **trong flash** (`PROGMEM`) thay vì RAM. Cú pháp `R"rawliteral( ... )rawliteral"` là "chuỗi thô": mọi ký tự bên trong (kể cả dấu ngoặc kép, xuống dòng) đều là văn bản bình thường, không cần "escape" — rất tiện cho HTML/JS.
- Bố cục trang (đọc theo thứ tự trong file):
  - Dòng 15–67: `<head>` + CSS. CSS định hình giao diện: thẻ thống kê, nút bấm, thanh trượt, badge trạng thái, tự co giãn theo màn hình điện thoại (`@media`). Biến `--primary:#2e7d32` (xanh lá)…, `*{box-sizing:border-box}` để kích thước phần tử tính cả viền cho dễ căn. Hai dòng chống lộ token qua Referer: dòng 18 `<meta name="referrer" content="no-referrer">` và dòng 20 thẻ `<link>` cdnjs kèm `referrerpolicy="no-referrer"` — token trên URL không bị gửi kèm khi trình duyệt tải CSS từ CDN.
  - Dòng 76–88: thẻ "Cảm biến" — 8 dòng hiển thị nhiệt độ, độ ẩm KK, ánh sáng, đất, nước, thời tiết, chuyển động, chạm. Mỗi giá trị có `id` (ví dụ `valTemp`) để JavaScript cập nhật.
  - Dòng 90–180: thẻ "Điều khiển" — các nút Bật/Tắt, Tự động/Thủ công cho bơm, đèn, quạt, RGB, đèn thở; thanh trượt servo; nút cối xay gió và cho ăn; hàng nút "chế độ tự động" tổng hợp; nút Trình diễn; thanh trạng thái 6 mục.
  - Mỗi nút gọi 1 hàm JavaScript qua `onclick=` — ví dụ `onclick="toggleDevice('pump')"`.
  - Dòng 183–225: `<script>` — "bộ não" phía trình duyệt.

### 12.3. JavaScript phía trình duyệt (dòng 184–224)

```javascript
const TOKEN=new URLSearchParams(location.search).get('token')||'';
function api(u){return TOKEN?(u+(u.indexOf('?')>=0?'&':'?')+'token='+encodeURIComponent(TOKEN)):u}
```

- Dòng 184–185: hai dòng "đầu não" của token. Mở trang kèm `?token=...` **một lần** (ví dụ `http://<ip>/?token=abc`) là `TOKEN` nhớ giá trị đó; hàm `api(u)` tự gắn `?token=` (hoặc `&token=` nếu URL đã có `?`) vào mọi lời gọi sau. Khi `WEB_TOKEN` rỗng (mặc định) thì `TOKEN` rỗng và `api()` trả URL nguyên gốc — UI không đổi hành vi.

```javascript
async function fetchData(){
try{
const r=await fetch('/api');
const d=await r.json();
window.__sf=d;
```

- Dòng 186–190: hàm chính cập nhật giao diện. `async/await` = chờ lấy dữ liệu mà không đơ trang. Gọi `/api`, đổi JSON thành đối tượng `d`, cất vào `window.__sf` để các hàm khác dùng lại.

```javascript
document.getElementById('valTemp').textContent=d.temp?.toFixed(1)??'--';
```

- Dòng 191–199: đổ số liệu cảm biến vào các ô chữ. `?.` = "nếu có thì dùng" (tránh lỗi khi thiếu), `??` = "nếu rỗng/không có thì thay bằng". Riêng thời tiết và PIR/chạm được đổi mã số thành chữ tiếng Việt qua bảng tra `wt[...]` và `? :`.

```javascript
const p=document.getElementById('badgePump');p.textContent=d.relay?'Bật':'Tắt';p.className='badge '+(d.relay?'on':'off');
```

- Dòng 200–204: cập nhật các "badge" trạng thái (Bật/Tắt, Đang quay/Đang cho ăn/Tắt) — đổi cả chữ lẫn màu (`class on` = xanh, `off` = xám).
- Dòng 205–212: cập nhật thanh trạng thái tự động + tên bước demo đang chạy + badge tự động RGB.

```javascript
}catch(e){console.error(e)}
}
```

- Dòng 213: nếu mạng lỗi thì ghi lỗi vào console trình duyệt (F12) — không làm treo giao diện.

```javascript
async function toggleDevice(id){await fetch(api('/toggle?id='+id));fetchData()}
async function ledSet(on){await fetch(api(on?'/on':'/off'));fetchData()}
async function toggleWindmill(){const s=window.__sf||{};await fetch(api('/windmill?on='+(s.windmill?'false':'true')));fetchData()}
async function feedNow(){await fetch(api('/feed'));fetchData()}
async function runDemo(){if(!confirm('Chạy trình diễn lần lượt các hoạt động (khoảng 40 giây)?'))return;await fetch(api('/demo'));fetchData()}
async function setAutoMode(id,on){await fetch(api('/auto?id='+id+'&on='+on));fetchData()}
async function setRgb(r,g,b){await fetch(api('/rgb?r='+r+'&g='+g+'&b='+b));fetchData()}
async function setServo(v){document.getElementById('servoLabel').textContent=v;await fetch(api('/servo?angle='+v))}
```

- Dòng 215–222: mỗi hàm gửi 1 yêu cầu HTTP tới route tương ứng rồi gọi `fetchData()` cập nhật lại màn hình. Mọi lệnh đổi trạng thái (`toggleDevice`, `ledSet`, `toggleWindmill`, `feedNow`, `runDemo`, `setAutoMode`, `setRgb`, `setServo`) đều bọc qua `api(...)` nên token (nếu có) tự động đi kèm. Vài điểm đáng chú ý:
  - `toggleWindmill` đọc trạng thái hiện tại từ `window.__sf` để gửi `on=true/false` (vì route cần giá trị tường minh, không tự đảo).
  - `runDemo` hỏi `confirm()` trước khi chạy — tránh bấm nhầm.
  - `setServo` cập nhật nhãn góc ngay lập tức cho mượt.

```javascript
setInterval(fetchData,2000);
fetchData();
```

- Dòng 223: tự cập nhật mỗi 2 giây. Dòng 224: gọi ngay 1 lần lúc mở trang.
- Nhờ hàm `fetchData` chạy nền, mọi số liệu trên web luôn tươi mà người dùng không phải bấm "tải lại".

### 12.4. Bảo vệ web — sameOriginUrl và guardWeb (dòng 235–266)

```cpp
static bool sameOriginUrl(const String &url, const String &origin) {
  if (url.length() == 0) return true;      // không gửi header → cho qua (curl, app tự viết)
  if (!url.startsWith(origin)) return false;
  if (url.length() == origin.length()) return true;
  char c = url.charAt(origin.length());    // biên giới host, tránh "http://<ip>.evil.com"
  return c == '/' || c == ':' || c == '?';
}
```

- Dòng 235–241: `sameOriginUrl` kiểm tra URL (Origin/Referer) có cùng nguồn với thiết bị không. URL rỗng (curl, app tự viết không gửi header) được cho qua; ngược lại URL phải bắt đầu bằng `"http://" + WiFi.localIP()`, và ký tự ngay sau host phải là `/`, `:` hoặc `?` — nhờ vậy tên miền lạm dụng tiền tố IP như `http://<ip>.evil.com` bị chặn.

```cpp
static bool guardWeb() {
  const String me = String("http://") + WiFi.localIP().toString();
  ...
}
```

- Dòng 243–266: `guardWeb()` là "người gác cổng" cho mọi lệnh đổi trạng thái:
  - Dòng 245–252: Origin hoặc Referer khác nguồn → **403 `forbidden`**.
  - Dòng 253–256: header `Sec-Fetch-Site: cross-site` → 403 (trình duyệt hiện đại gắn nhãn này khi trang khác gọi vào).
  - Dòng 257–264: nếu `WEB_TOKEN` (trong `src/config.h`) khác rỗng thì bắt buộc kèm token qua header `X-Auth-Token` hoặc tham số `?token=`; thiếu/sai → **401 `unauthorized`**.
- 11 handler đổi trạng thái (`handleToggle`, `handleAuto`, `handleRgb`, `handleBreatheLed`, `handleLedOn`, `handleLedOff`, `handleServo`, `handleWindmill`, `handleFeed`, `handleDemo`, `handleThreshold`) đều gọi `guardWeb()` ngay dòng đầu. Các route `/` (trang chủ), `/api` (chỉ đọc) và favicon **không** guard — chỉ lệnh điều khiển mới bị chặn.
- Phụ thuộc phần cứng (thư viện WebServer): `server.collectHeaders()` trong `setup()` của `SmartFarm.ino` (dòng 154–157, xem PHẦN 1) mới đăng ký các header `Origin`, `Referer`, `Sec-Fetch-Site`, `X-Auth-Token`. Thư viện chỉ giữ header đã đăng ký; thiếu dòng đó thì mọi check header ở trên vô hiệu (luôn coi như "không có header").

### 12.5. handleRoot — trang chủ (dòng 268–272)

```cpp
void handleRoot() {
  MOD_DEBUG(Webserver, "GET /");
  server.sendHeader("X-Frame-Options", "DENY");   // chống nhúng iframe (clickjacking)
  server.send_P(200, "text/html", ROOT_HTML);
}
```

- Dòng 269: ghi log chi tiết nếu công tắc debug Webserver bật.
- Dòng 270: gửi thêm header `X-Frame-Options: DENY` — cấm nhúng trang vào `<iframe>` (chống clickjacking).
- Dòng 271: gửi trang HTML với mã **200 = OK**. `send_P` = gửi thẳng từ flash, không copy vào RAM — tiết kiệm bộ nhớ.

### 12.6. handleApi — dữ liệu JSON (dòng 275–302)

```cpp
void handleApi() {
  MOD_DEBUG(Webserver, "GET /api");

  // Dùng giá trị global đã lọc — không đọc cảm biến thô ở đây
  // (DHT11 cần 1 giây giữa hai lần đọc; đọc thêm sẽ trả dữ liệu cũ)
  // Dựng JSON bằng snprintf vào buffer cố định — không tự nối String trong handler
  // và không cấp phát heap thêm (send() của thư viện vẫn tạo String nội bộ một lần).
  char buf[512];
  snprintf(buf, sizeof(buf),
           "{\"temp\":%.1f,\"hum\":%.1f,\"light\":%d,\"soil\":%d,\"water\":%d,"
           "\"weather\":%d,\"pir\":%s,\"touch\":%s,\"relay\":%s,\"fan\":%s,"
           "\"led\":%s,\"breathe\":%s,\"windmill\":%s,\"feed\":%s,\"demo\":%s,"
           "\"demoStep\":\"%s\",\"pumpAuto\":%s,\"lightAuto\":%s,\"rgbAuto\":%s,"
           "\"fanAuto\":%s,\"breatheAuto\":%s}",
           (double)gTemp, (double)gHum, (int)gLight, (int)gSoil, (int)gWater,
           (int)weatherStatus(),
           gPir ? "true" : "false", gTouch ? "true" : "false",
           relayState ? "true" : "false", fanState ? "true" : "false",
           blueLedState ? "true" : "false", breatheLedState ? "true" : "false",
           windmillOn ? "true" : "false", feedRunning() ? "true" : "false",
           demoRunning() ? "true" : "false",
           demoRunning() ? demoStepName() : "",
           pumpAuto ? "true" : "false", lightAuto ? "true" : "false",
           rgbAuto ? "true" : "false", fanAuto ? "true" : "false",
           breatheAuto ? "true" : "false");

  server.send(200, "application/json", buf);
}
```

- Dòng 276: log debug.
- Dòng 278–281: comment giải thích vì sao chỉ dùng biến global đã lọc (không đọc cảm biến thô — DHT11 cần 1 giây giữa hai lần đọc), **không tự nối `String`** trong handler và **không cấp phát heap thêm**; chỉ `send()` của thư viện vẫn tạo một `String` nội bộ đúng một lần.
- Dòng 282–299: dựng JSON bằng `char buf[512]` + **một** `snprintf` duy nhất (thay cho nối `String` + `reserve(520)` trước đây). Buffer 512 byte đủ chứa JSON 21 khóa (kể cả bước demo dài nhất); giới hạn này đã được kiểm chứng bằng test hợp đồng JSON, và `snprintf` tự cắt an toàn nếu có gì vượt.
- Dòng 284–288: chuỗi định dạng — 6 giá trị cảm biến (`%.1f` cho nhiệt độ/độ ẩm, `%d` cho ánh sáng/đất/nước và thời tiết mã 0–3) cùng 14 giá trị boolean + 1 chuỗi `demoStep`, in đúng chữ `true`/`false` theo chuẩn JSON (không phải 1/0).
- Dòng 289–299: danh sách giá trị truyền vào `snprintf` (biến global đã lọc, `weatherStatus()`, các cờ trạng thái).
- Dòng 296: `demoStep` là chuỗi — khi không chạy demo thì truyền chuỗi rỗng `""`.
- Lưu ý: dấu `\"` trong chuỗi định dạng nghĩa là "in ra ký tự ngoặc kép" (vì ngoặc kép không dấu huyền đã dùng để bao chuỗi C++).
- Dòng 301: gửi buffer với mã 200 và kiểu nội dung `application/json`. Trình duyệt sẽ tự hiểu đây là dữ liệu, không phải trang web.

```cpp
static uint8_t clampByte(long v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}
```

- Dòng 304–308: "kẹp" giá trị vào khoảng 0…255. Người dùng có thể gõ `r=999` hay `r=-5`; hàm này bảo đảm màu hợp lệ trước khi gửi xuống phần cứng.

### 12.7. handleToggle — nút Bật/Tắt (dòng 311–333)

```cpp
void handleToggle() {
  if (!guardWeb()) return;
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

- Dòng 312: gọi `guardWeb()` — chặn ngay (403/401) nếu yêu cầu không hợp lệ (xem 12.4).
- Dòng 313: đọc tham số `id` từ URL, ví dụ `/toggle?id=pump`.
- Dòng 314–316 (bơm): **tắt chế độ tự động trước**, rồi đảo trạng thái bơm. Quy tắc chung của cả dự án: điều khiển thủ công luôn tắt tự động tương ứng — để máy không "giằng co" với người.
- Dòng 317–319: đèn xanh tương tự (đảo + tắt lightAuto).
- Dòng 320–322: quạt.
- Dòng 323–326 (đèn thở): tính trạng thái mong muốn **trước khi** tắt tự động (vì hàm `setBreatheLed` đọc trạng thái hiện tại), rồi chuyển chế độ thủ công và áp dụng.
- Dòng 327–330: `id` lạ → trả mã **400 = yêu cầu sai**.
- Dòng 331–332: lưu cài đặt (cờ tự động thay đổi) rồi trả `200 OK` với chữ "ok".

### 12.8. handleAuto — bật/tắt tự động (dòng 336–353)

```cpp
void handleAuto() {
  if (!guardWeb()) return;
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

- Dòng 337: gọi `guardWeb()` như mọi handler đổi trạng thái.
- Dòng 339: `on` chỉ là `true` khi chuỗi tham số **đúng chữ** "true" — mọi giá trị khác (kể cả "1") coi là false. Nhờ vậy hành vi rõ ràng, dễ test.
- Dòng 340–346: gán cờ theo `id`. Riêng "breathe": bật tự động thì **bật đèn thở luôn** (để người dùng thấy ngay), tắt thì tắt đèn.
- Dòng 347–350: `id` lạ → 400.
- Dòng 351–352: lưu flash + trả ok.

### 12.9. handleRgb — đổi màu LED RGB (dòng 356–365)

```cpp
void handleRgb() {
  if (!guardWeb()) return;
  uint8_t r = clampByte(server.arg("r").toInt());
  uint8_t g = clampByte(server.arg("g").toInt());
  uint8_t b = clampByte(server.arg("b").toInt());
  setRgbAuto(false);
  setRgb(r, g, b);
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 357: gọi `guardWeb()`.
- Dòng 358–360: đọc 3 kênh màu, đổi chuỗi thành số (`toInt`), kẹp 0…255. Ô trống trả 0 → màu đen.
- Dòng 361–362: chọn màu thủ công thì tắt tự động RGB; sau đó đặt màu.

### 12.10. handleBreatheLed — nút riêng cho đèn thở (dòng 368–375)

```cpp
void handleBreatheLed() {
  if (!guardWeb()) return;
  bool want = !breatheLedState;
  setBreatheAuto(false);
  setBreatheLed(want);
  saveSettings();
  server.send(200, "text/plain", "ok");
}
```

- Dòng 369: gọi `guardWeb()`.
- Dòng 370–372: lưu ý thứ tự: tính `want` trước, tắt tự động, rồi mới áp dụng. (Bug cũ "đèn thở không tắt được từ web" là do làm sai thứ tự — bộ test kiểm tra đúng chuỗi này.)

### 12.11. handleLedOn / handleLedOff — route `/on` `/off` (dòng 379–395)

```cpp
void handleLedOn() {
  if (!guardWeb()) return;
  MOD_DEBUG(Webserver, "GET /on");
  setLightAuto(false);
  setBlueLed(true);
  saveSettings();
  server.send(200, "text/plain", "LED ON");
}

void handleLedOff() {
  if (!guardWeb()) return;
  MOD_DEBUG(Webserver, "GET /off");
  setLightAuto(false);
  setBlueLed(false);
  saveSettings();
  server.send(200, "text/plain", "LED OFF");
}
```

- Đây là 2 route "kinh điển" của bài học (bật/tắt đèn từ xa). Cả hai gọi `guardWeb()` ngay đầu hàm (dòng 380 và 389), tắt tự động đèn trước, rồi bật/tắt, lưu flash, trả lời bằng tiếng Anh ngắn `LED ON` / `LED OFF` (khớp tài liệu kit).

### 12.12. handleServo — điều khiển góc servo (dòng 398–408)

```cpp
void handleServo() {
  if (!guardWeb()) return;
  int angle = server.arg("angle").toInt();
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  setWindmill(false);   // điều khiển servo thủ công sẽ nhả cối xay gió và cửa cho ăn
  stopFeed();
  servoAngle = angle;
  servo.write(servoAngle);
  server.send(200, "text/plain", "ok");
}
```

- Dòng 399: gọi `guardWeb()`.
- Dòng 400–402: đọc góc, kẹp vào 0…180 — servo chỉ quay được trong khoảng này, nhập ngoài khoảng sẽ kẹt bánh răng.
- Dòng 403–404: điều khiển tay thì **nhả cả 2 cơ cấu tự động** dùng chung servo (cối xay + máng ăn), nếu không servo sẽ bị 2 lệnh giành nhau.
- Dòng 405–406: nhớ góc vào biến chung rồi gửi ra servo.

### 12.13. handleWindmill — cối xay gió (dòng 411–416)

```cpp
void handleWindmill() {
  if (!guardWeb()) return;
  bool on = server.arg("on") == "true";
  setWindmill(on);
  server.send(200, "text/plain", on ? "windmill on" : "windmill off");
}
```

- Dòng 412: gọi `guardWeb()`.
- Dòng 413–415: bật/tắt cối xay theo tham số tường minh `on=true|false`; trả lời theo trạng thái vừa áp dụng. Việc bật cối xay sẽ tự nhả máng ăn (đã xử lý trong `setWindmill`).

### 12.14. handleFeed — cho ăn (dòng 419–423)

```cpp
void handleFeed() {
  if (!guardWeb()) return;
  startFeed();
  server.send(200, "text/plain", "feeding");
}
```

- Dòng 420: gọi `guardWeb()`.
- Dòng 421–422: mở cửa; việc đóng sau 2 giây do `controlFeed()` trong vòng lặp chính lo — bấm nút này không "khóa" web 2 giây.

### 12.15. handleDemo — trình diễn (dòng 426–431)

```cpp
void handleDemo() {
  if (!guardWeb()) return;
  MOD_DEBUG(Webserver, "GET /demo");
  startDemo();
  server.send(200, "text/plain", "demo started");
}
```

- Dòng 427: gọi `guardWeb()`.
- Dòng 429–430: khởi động trình diễn và trả lời ngay. Trình diễn chạy nền trong vòng lặp chính, không làm treo web.

### 12.16. handleThreshold — chỉnh ngưỡng (dòng 434–457)

```cpp
void handleThreshold() {
  if (!guardWeb()) return;
  bool changed = false;
  if (server.hasArg("soilLow"))  { soilThresholdLow  = constrain(server.arg("soilLow").toFloat(),  0.0f, 4095.0f); changed = true; }
  if (server.hasArg("soilHigh")) { soilThresholdHigh = constrain(server.arg("soilHigh").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("waterLow")) { waterThresholdLow = constrain(server.arg("waterLow").toFloat(), 0.0f, 4095.0f); changed = true; }
  if (server.hasArg("light"))    { lightThreshold    = constrain(server.arg("light").toInt(),      0,    4095);   changed = true; }
  if (server.hasArg("tempHigh")) { tempThresholdHigh = constrain(server.arg("tempHigh").toFloat(), 0.0f, 60.0f);   changed = true; }
  if (server.hasArg("tempLow"))  { tempThresholdLow  = constrain(server.arg("tempLow").toFloat(),  0.0f, 60.0f);   changed = true; }
```

- Dòng 435: gọi `guardWeb()`.
- Dòng 437–442: với **từng** tham số có mặt trong URL, đọc và kẹp vào khoảng hợp lệ:
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
    tempThresholdHigh = tempThresholdLow + 1.0f;  // không chặn trên: giữ high > low tại mép 60 C
  }
  saveSettings();
  LOG_INFO("Thresholds updated from web");
  server.send(200, "text/plain", "ok");
}
```

- Dòng 444–447: không có tham số nào → trả 400 "no params".
- Dòng 448–450: nếu người dùng đặt ngưỡng đất "cao" thấp hơn ngưỡng "thấp" → **tự sửa** thành thấp + 50 (kẹp tối đa 4095). Nhờ vậy web không thể tạo ra cấu hình làm bơm rung. (Ví dụ test: đặt soilLow=4095 thì soilHigh cũng thành 4095 — sát trần thì buộc phải bằng nhau, controlPump vẫn có chốt riêng chống rung.)
- Dòng 451–453: tương tự cho nhiệt độ nhưng cộng 1 và **không kẹp trần**: nếu tempLow=60 thì high thành 61 — vẫn giữ đúng bất biến high > low (test khóa hành vi này).
- Dòng 454–456: lưu flash, ghi log, trả ok.

---

## PHẦN 13 — Thư mục test/ (bộ kiểm thử 772.362 lượt)

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
./test/run_tests.sh --full       # 5 bản build (mặc định)
./test/run_tests.sh --coverage   # đo độ phủ dòng lệnh (gcov)
```

5 bản build của `--full` chạy CÙNG bộ test với cấu hình khác nhau, để chắc mọi nhánh
`#if` đều đúng. Mọi bản build đều kèm `-DSMARTFARM_TEST_HOOKS` để bộ test reset được cả
các biến static trong `src/` (xem các hook `testReset*Statics` ở PHẦN 5–8, 11). Bản
`webtoken` chạy thêm test D16 nên có **772.379** lượt kiểm tra thay vì 772.362:

| Bản build | Cấu hình | Kiểm tra điều gì |
|---|---|---|
| `core3` | Mặc định kit, core 3.x | Đường đi chính |
| `core2-invert` | Core 2.x + 3 cờ đảo cảm biến | Tương thích ngược + cảm biến đọc ngược chiều |
| `passive-lowrelay` | Còi thụ động + relay kích mức thấp | Nhánh `tone()` và nhánh đảo mức relay |
| `asan+ubsan` | Địa chỉ + hành vi không xác định | Bắt tràn bộ nhớ, dùng biến chưa khởi tạo… |
| `webtoken` | `-DWEB_TOKEN='"token-bao-mat-test"'` | Bảo vệ token web: thiếu/sai token → 401, đúng qua header/`?token=` → 200 |

### 13.3. `test_main.cpp` gồm những gì?

- **T1–T19**: test đơn vị từng mô-đun — ví dụ T3 kiểm tra bơm bật khi đất khô, giữ nguyên khi đất trong khoảng trễ, tắt khi đất ẩm; T16 kiểm tra PIR không tắt oan đèn bật thủ công và đèn thở được khôi phục; T18 kiểm tra reset mặc định, bỏ qua ghi flash khi giá trị không đổi và mã bảo mật không bị ghi vào NVS; T19 kiểm tra logger với cấp log ngoài enum.
- **D1–D3**: soi đúng các giá trị **biên** — ví dụ `gWater` đúng bằng ngưỡng thì *được phép* chạy bơm (luật là `<` chứ không phải `<=`), hoặc van thời gian: đúng 29 ms chưa đổi nhịp, đúng 30 ms mới đổi.
- **I1–I13**: test tích hợp với `setup()` + `loop()` thật — boot đăng ký đủ 14 route, thứ tự ghi mức điện trước `pinMode` (chống glitch relay), mất WiFi tự nối lại, boot thiếu OLED/không có WiFi vẫn chạy, cấu hình WiFi động qua web (I12), callback OTA in đúng trạng thái (I13), v.v.
- **D5–D16**: test chuyên sâu — hợp đồng JSON 21 khóa với bộ đọc JSON tự viết, đúng từng ký tự thân phản hồi, OLED từng dòng và giới hạn 21 ký tự, menu serial với byte rác, trình diễn đúng 15 bước/30.700 ms, chống spam WiFi đúng 10.000 ms, bền vững qua mất điện, **fuzz 90.000 bước ngẫu nhiên** với các bất biến an toàn (bơm không bao giờ chạy khô, chân luôn khớp trạng thái…), tràn `millis()` cho mọi bộ đếm, chặn CSRF (Origin/Referer khác nguồn hoặc `Sec-Fetch-Site: cross-site` → 403) và bảo vệ token web tùy chọn (thiếu/sai → 401, đúng → 200).

### 13.4. Vài bất biến (invariant) quan trọng được kiểm tra liên tục

1. `!(sys.gWater < sys.waterThresholdLow && sys.relayState)` — không bao giờ bơm chạy khô.
2. Mức điện chân relay/quạt luôn khớp cờ trạng thái (kể cả khi đảo cực).
3. Cối xay gió và cửa máng ăn không bao giờ chạy đồng thời (chung server).
4. Đèn xanh "sáng đều" và "đèn thở" không bao giờ cùng bật.
5. Góc servo luôn trong 0…180; không có lượt ghi PWM/NeoPixel nào sai chỉ số.
6. Khi đèn tắt hẳn, PWM phải bằng 0 (không kẹt mức sáng cũ).

Từng dòng của chính bộ test được giải thích chi tiết ở **PHẦN 14** (`test_main.cpp`),
**PHẦN 15** (thư mục `test/mocks`) và **PHẦN 16** (`test/run_tests.sh`).

---

## PHẦN 14 — test/test_main.cpp (giải thích từng dòng)

`test/test_main.cpp` là bộ kiểm thử (test) tự động của firmware Smart Farm QE002, chạy được ngay trên máy tính (host/PC) mà không cần cắm kit ESP32. Bí quyết nằm ở thư mục `test/mocks/`: các hàm phần cứng như `digitalWrite()`, `analogRead()`, `millis()`, `ledcWrite()`... được thay bằng bản giả lập (mock — "đồ giả") ghi lại mọi giá trị để test kiểm tra. Nhờ vậy firmware thật (`SmartFarm.ino` + `src/*.cpp`) vẫn được biên dịch nguyên vẹn, còn bộ test chỉ cần "giả vờ" cắm cảm biến, đèn, relay rồi soi kết quả.

### 14.1. Khung xương bộ test và các hàm phụ trợ (dòng 1–189)

#### Comment đầu file (dòng 1–12)

```cpp
// Bộ test chạy trên host chuyên sâu cho firmware Smart Farm QE002.
//
// Bộ test biên dịch firmware THẬT (SmartFarm.ino + src/*.cpp) dựa trên các
// mock phần cứng điều khiển được trong test/mocks/ và thực hiện:
//   - unit test cho từng module logic điều khiển
//   - integration test qua setup()/loop() thật và các HTTP handler
//   - deep test: JSON contract nghiêm ngặt, giá trị biên, millis() wrap,
//     an toàn giữa bước của demo, bộ đếm throttle WiFi, hành vi debug-mode,
//     lưu settings bền qua một chu kỳ nguồn mô phỏng, và một lượt
//     fuzz/soak có seed kiểm tra các safety invariant sau mỗi loop().
//
// Cách build & chạy:  ./test/run_tests.sh   (xem test/README.md)
```

- Dòng 1: giới thiệu mục đích file — bộ test "chuyên sâu" chạy trên host (máy tính), không chạy trên kit.
- Dòng 2: dòng trống trong comment, dùng để ngắt các đoạn cho dễ đọc.
- Dòng 3–4: điểm cốt lõi — bộ test **biên dịch firmware thật** chứ không chép lại logic; phần cứng được thay bằng mock điều khiển được trong `test/mocks/`.
- Dòng 5: `unit test` — test từng module logic riêng lẻ (cảm biến, điều khiển, demo...).
- Dòng 6: `integration test` — test ghép: gọi thẳng `setup()`/`loop()` thật và các hàm xử lý HTTP thật.
- Dòng 7–10: liệt kê các nhóm "deep test" (test sâu): kiểm tra hợp đồng JSON (cấu trúc dữ liệu API trả về), giá trị biên, hiện tượng `millis()` tràn số, an toàn khi demo chuyển bước, bộ đếm chống spam WiFi, hành vi chế độ debug, cài đặt lưu bền qua một chu kỳ tắt/mở nguồn mô phỏng, và fuzz/soak (chạy lâu, dữ liệu ngẫu nhiên) để kiểm tra các bất biến an toàn sau mỗi `loop()`.
- Dòng 11: dòng trống.
- Dòng 12: hướng dẫn cách build (biên dịch) và chạy: `./test/run_tests.sh`, chi tiết xem `test/README.md`.

#### Các `#include` và khai báo entry point (dòng 13–34)

```cpp
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <random>

#include "mock.h"
#include "globals.h"
#include "sensors.h"
#include "actuators.h"
#include "control.h"
#include "demo.h"
#include "settings.h"
#include "oled.h"
#include "config.h"

// Các entry point thật của firmware (SmartFarm.ino)
void setup();
void loop();
```

- Dòng 13–20: nhóm thư viện chuẩn của C++ (dấu `<...>` nghĩa là thư viện hệ thống):
  - Dòng 13 `cstdio`: cho `printf` — in thông báo PASS/FAIL ra màn hình.
  - Dòng 14 `cmath`: cho `fabs` (trị tuyệt đối số thực) phục vụ macro so sánh gần đúng.
  - Dòng 15 `cstring`: cho `strstr` (tìm chuỗi con), `strcmp` (so sánh chuỗi), `strlen` (độ dài chuỗi).
  - Dòng 16 `cstdlib`: các hàm tiện ích C (đổi chuỗi thành số, thoát chương trình...).
  - Dòng 17 `string`: lớp `std::string` tiện dụng khi ghép/so sánh chuỗi trong test.
  - Dòng 18 `vector`: mảng động `std::vector` để chứa danh sách kết quả, tham số...
  - Dòng 19 `set`: tập hợp `std::set` (mỗi phần tử chỉ xuất hiện một lần) thường dùng để kiểm tra trùng lặp.
  - Dòng 20 `random`: bộ sinh số ngẫu nhiên cho phần fuzz/soak với seed (hạt giống) cố định để tái lập được kết quả.
- Dòng 22–30: nhóm header của chính dự án (dấu `"..."` nghĩa là file trong repo):
  - Dòng 22 `mock.h`: khai báo toàn bộ hàm/biến mock phần cứng — trái tim của bộ test.
  - Dòng 23 `globals.h`: các biến trạng thái toàn cục của firmware mà test cần đọc/ghi trực tiếp.
  - Dòng 24–29: các module chức năng thật — cảm biến, cơ cấu chấp hành (relay, LED...), điều khiển tự động, demo, lưu cài đặt, màn hình OLED.
  - Dòng 30 `config.h`: để test đọc được hằng số cấu hình `WEB_TOKEN` — nhờ đó `invoke()` biết bản build có bật bảo vệ token hay không (xem D16 ở mục 14.12).
- Dòng 32: comment nói rõ hai hàm bên dưới là entry point thật của firmware, nằm trong `SmartFarm.ino`.
- Dòng 33–34: **khai báo trước** (prototype) `setup()` và `loop()`. Trong Arduino, hai hàm này nằm trong file `.ino` — không có file header nào khai báo chúng. Muốn `test_main.cpp` gọi được `setup()`/`loop()` thật thì phải tự khai báo hai prototype này; còn phần thân hàm do `SmartFarm.ino` cung cấp lúc liên kết (link) chương trình. Nhờ vậy test chạy đúng chu trình khởi động và vòng lặp như trên kit.

#### Biến đếm và 4 macro kiểm tra (dòng 36–64)

```cpp
static int gChecks = 0, gFails = 0;
```

- Dòng 36: hai biến đếm dùng chung cho cả file. `static` ở đây nghĩa là chỉ file này nhìn thấy (không xung đột với file khác). Chữ `g` là quy ước đặt tên cho biến toàn cục (global). `gChecks` đếm tổng số lần kiểm tra đã chạy, `gFails` đếm số lần kiểm tra thất bại. Cuối chương trình, hàm `main()` (ở phần sau của file) sẽ đọc hai số này để quyết định PASS/FAIL toàn bộ.

```cpp
#define CHECK(cond) do { \
  gChecks++; \
  if (!(cond)) { gFails++; printf("  FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

#define CHECK_EQ(a, b) do { \
  gChecks++; \
  if ((long)(a) != (long)(b)) { \
    gFails++; printf("  FAIL line %d: %s (%ld) != %s (%ld)\n", \
                     __LINE__, #a, (long)(a), #b, (long)(b)); \
  } \
} while (0)

#define CHECK_NEAR(a, b, eps) do { \
  gChecks++; \
  if (fabs((double)(a) - (double)(b)) > (eps)) { \
    gFails++; printf("  FAIL line %d: %s (%.2f) != %s (%.2f)\n", \
                     __LINE__, #a, (double)(a), #b, (double)(b)); \
  } \
} while (0)

#define CHECK_STR(a, b) do { \
  gChecks++; \
  if (strcmp((a), (b)) != 0) { \
    gFails++; printf("  FAIL line %d: \"%s\" != \"%s\"\n", __LINE__, (a), (b)); \
  } \
} while (0)
```

- Dòng 38–41 — `CHECK(cond)`: macro cơ bản nhất. Macro là "đoạn mã được dán tự động" vào chỗ gọi trước khi biên dịch; ở đây nó làm 3 việc: (1) tăng `gChecks` vì vừa có thêm một phép kiểm tra; (2) nếu điều kiện `cond` **sai** thì tăng `gFails` và in dòng `FAIL`; (3) in kèm `__LINE__` — một macro có sẵn của trình biên dịch, tự thay bằng **số dòng hiện tại** nơi lời gọi `CHECK` nằm, giúp biết ngay test hỏng ở đâu; và `#cond` — toán tử "stringize" (`#` đứng trước tham số macro), biến chính biểu thức điều kiện thành **chuỗi ký tự** để in ra nguyên văn bạn đã kiểm tra cái gì.
- Dòng 38 & 40 — `do { ... } while (0)`: kỹ thuật gói nhiều câu lệnh vào **một câu lệnh duy nhất**. Nếu không có nó, đoạn `if (x) CHECK(y); else ...` sẽ bị hiểu sai (phần `else` dính vào `if` bên trong macro). `while (0)` nghĩa là vòng lặp chạy đúng **một lần** rồi thoát — chỉ để đủ cú pháp, không phải lặp thật. Dấu `\` cuối mỗi dòng là "nối dòng": macro viết trên nhiều dòng nhưng trình biên dịch đọc như một dòng.
- Dòng 43–49 — `CHECK_EQ(a, b)`: như `CHECK` nhưng dành cho **so sánh bằng**. Điểm đáng chú ý là `(long)(a)` và `(long)(b)`: ép cả hai vế về kiểu số nguyên `long` trước khi so. Vì sao? Vì trong firmware các giá trị có nhiều kiểu khác nhau (`bool`, `int`, `uint32_t`, `char`...); ép chung về `long` để tránh cảnh báo "so sánh khác dấu" (signed/unsigned) và tránh kết quả so sánh kỳ lạ khi trộn kiểu. Khi sai, `%ld` in ra **giá trị thật** của hai vế cạnh tên biểu thức (`#a`, `#b`), ví dụ `FAIL line 250: relayState (0) != 1 (1)`.
- Dòng 51–57 — `CHECK_NEAR(a, b, eps)`: dành cho **số thực** (nhiệt độ, độ ẩm...). Máy tính lưu số thực có sai số nhỏ, nên không so bằng trực tiếp mà kiểm tra `|a − b| > eps` thì mới fail. `fabs((double)(a) - (double)(b))` là trị tuyệt đối của hiệu, ép cả hai về `double` cho chính xác. `eps` (epsilon) là "sai số cho phép" — test tự chọn, ví dụ 0.05. Khi in dùng `%.2f` (2 chữ số thập phân).
- Dòng 59–64 — `CHECK_STR(a, b)`: dành cho **chuỗi C** (`const char*`). Không dùng `==` (vì `==` chỉ so địa chỉ ô nhớ, không so nội dung) mà dùng `strcmp`: hàm này trả về 0 khi hai chuỗi **giống hệt nhau**, khác 0 khi khác. Vì vậy điều kiện fail là `strcmp(...) != 0`. Khi in, giá trị được bọc trong dấu ngoặc kép để thấy rõ khoảng trắng/ký tự lạ.
- Tóm tắt cách dùng: số nguyên/nhị phân dùng `CHECK_EQ`; số thực dùng `CHECK_NEAR`; chuỗi dùng `CHECK_STR`; và `CHECK` cho mọi điều kiện logic khác (`>`, `<`, đã gọi hàm chưa...).

#### Các hàm phụ trợ (dòng 66–189)

```cpp
// ==============================================
//  Các hàm phụ trợ
// ==============================================

static bool has(const char *hay, const char *needle) {
  return hay && needle && strstr(hay, needle) != nullptr;
}
```

- Dòng 66–68: khối comment trang trí, đánh dấu bắt đầu vùng "hàm phụ trợ" (helper) — các tiện ích nhỏ dùng cho nhiều test.
- Dòng 70: `has` = "có chứa không", kiểu trả về `bool` (đúng/sai), `static` để chỉ dùng trong file. Tên tham số vay từ thành ngữ "tìm cây kim trong đống cỏ khô": `hay` (đống cỏ) là chuỗi bị tìm, `needle` (cây kim) là chuỗi cần tìm. Dấu `*` nghĩa là con trỏ — trỏ tới chuỗi ký tự.
- Dòng 71: trả về true nếu: `hay` khác null (không rỗng/không trỏ hư) **và** `needle` khác null **và** `strstr(hay, needle)` tìm thấy (trả về khác `nullptr`). Phép `&&` dừng sớm từng vế nên `strstr` chỉ chạy khi cả hai chuỗi hợp lệ — tránh crash. Hàm này hầu như luôn dùng để kiểm tra chuỗi JSON có chứa một trường nào đó.

```cpp
static int countOccurrences(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 0;
  int n = 0;
  for (const char *p = hay; (p = strstr(p, needle)) != nullptr; p += strlen(needle)) n++;
  return n;
}
```

- Dòng 74: đếm số lần `needle` xuất hiện trong `hay`.
- Dòng 75: "gác cổng" — nếu thiếu chuỗi hoặc `needle` rỗng (`*needle` là ký tự đầu tiên, bằng 0 nghĩa là chuỗi rỗng) thì trả 0. Phải chặn `needle` rỗng vì `strstr` với chuỗi rỗng sẽ luôn tìm thấy và vòng lặp chạy vô tận.
- Dòng 76: `n` là biến đếm, bắt đầu từ 0.
- Dòng 77: một dòng `for` gói cả vòng lặp: con trỏ `p` bắt đầu ở đầu chuỗi `hay`; mỗi lượt gọi `strstr(p, needle)` — nếu tìm thấy, `p` nhảy tới vị trí đó và điều kiện đúng (khác `nullptr`), thân vòng lặp tăng `n`; hết thân, `p += strlen(needle)` nhảy **qua khỏi** chuỗi vừa tìm để lần sau tìm tiếp phần còn lại (nhờ vậy không đếm chồng cùng một vị trí). Vòng lặp dừng khi không còn tìm thấy.
- Dòng 78: trả về số lần đếm được. Dùng để kiểm tra ví dụ trường `"relay"` xuất hiện đúng **1 lần** trong JSON.

```cpp
static uint32_t currentLedPwm() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  return mockPwm[BLUE_LED_PIN];
#else
  return mockPwm[LED_PWM_CHANNEL];
#endif
}
```

- Dòng 81: hàm trả về giá trị PWM (độ sáng) hiện tại của LED xanh dương, kiểu `uint32_t` (số nguyên không dấu 32 bit).
- Dòng 82: `#if ESP_ARDUINO_VERSION_MAJOR >= 3` — đây là **biên dịch có điều kiện** (không phải `if` chạy lúc runtime): trình biên dịch xem phiên bản core ESP32 Arduino đang dùng. Từ core **v3**, API PWM `ledc` thay đổi: điều khiển theo **chân** thay vì theo **kênh**.
- Dòng 83: nhánh core v3 — đọc `mockPwm[BLUE_LED_PIN]`, tức mock ghi giá trị PWM theo chân LED xanh.
- Dòng 84: `#else` — các core cũ hơn (2.x).
- Dòng 85: nhánh cũ — đọc `mockPwm[LED_PWM_CHANNEL]`, tức theo kênh LEDC.
- Dòng 86: `#endif` kết thúc khối điều kiện. Nhờ vậy cùng một bộ test chạy đúng trên cả hai môi trường build.

```cpp
static void checkRelayPin(bool on) {
#if RELAY_ACTIVE_LOW
  CHECK_EQ(mockPinLevel[RELAY_PIN], on ? LOW : HIGH);
#else
  CHECK_EQ(mockPinLevel[RELAY_PIN], on ? HIGH : LOW);
#endif
}
```

- Dòng 89: kiểm tra chân điều khiển relay có đúng trạng thái mong muốn `on` (bật/tắt) không. `void` nghĩa là không trả về gì.
- Dòng 90: `#if RELAY_ACTIVE_LOW` — relay của board có thể là loại "kích mức thấp" (active-low): bật khi chân ở 0V. Cờ này do `config.h` định nghĩa.
- Dòng 91: nếu active-low: bật (`on = true`) thì chân phải là `LOW`, tắt thì `HIGH`. Đây là phép đảo logic quen thuộc của relay.
- Dòng 92–94: `#else`/`#endif` — nếu relay "kích mức cao" (active-high) thì ngược lại: bật ⇒ `HIGH`, tắt ⇒ `LOW`. Dùng `CHECK_EQ` để so với `mockPinLevel[RELAY_PIN]` — mảng mock ghi lại mức chân từ lần `digitalWrite` cuối.

```cpp
static void checkBuzzer(bool on) {
#if BUZZER_PASSIVE
  if (on) CHECK(mockBuzzerToneHz > 0);
  else    CHECK_EQ(mockBuzzerToneHz, 0);
#else
  CHECK_EQ(mockPinLevel[BUZZER_PIN], on ? HIGH : LOW);
#endif
}
```

- Dòng 97: kiểm tra còi báo đang bật/tắt đúng như mong muốn.
- Dòng 98: `#if BUZZER_PASSIVE` — board có thể dùng **còi thụ động** (passive): loại còi cần tín hiệu tần số (hàm `tone()`) mới kêu, kêu to nhỏ theo tần số.
- Dòng 99: nếu muốn bật: `mockBuzzerToneHz` (tần số tone ghi lại được) phải **lớn hơn 0** — có phát tần số nghĩa là đang kêu.
- Dòng 100: nếu muốn tắt: tần số phải bằng 0 (im lặng). Dùng `CHECK_EQ` cho trường hợp này.
- Dòng 101–103: `#else` — nếu là **còi chủ động** (active): chỉ có bật/tắt bằng mức chân, nên kiểm tra `mockPinLevel[BUZZER_PIN]` là `HIGH`/`LOW` tương ứng.

```cpp
#ifdef SMARTFARM_TEST_HOOKS
// Hook do firmware cung cấp (chỉ tồn tại ở bản build test): trả các biến
// static trong src/ về trạng thái vừa cấp điện → test độc lập thứ tự hơn.
void testResetActuatorStatics();
void testResetControlStatics();
void testResetDemoStatics();
void testResetSensorStatics();
void testResetWifiStatics();
#endif

static void resetState() {
  mockReset();
#ifdef SMARTFARM_TEST_HOOKS
  testResetActuatorStatics();
  testResetControlStatics();
  testResetDemoStatics();
  testResetSensorStatics();
  testResetWifiStatics();
#endif
  soilThresholdLow = 400; soilThresholdHigh = 700; waterThresholdLow = 200;
  lightThreshold = 500; tempThresholdHigh = 30; tempThresholdLow = 25;
  pumpAuto = lightAuto = rgbAuto = fanAuto = true;
  breatheAuto = false;
  relayState = fanState = blueLedState = breatheLedState = false;
  windmillOn = false;
  rgbR = rgbG = rgbB = 0;
  breatheBrightness = 0;
  breatheDirection = 1;
  servoAngle = 0;
  gTemp = 22; gHum = 50; gLight = 300; gSoil = 500; gWater = 800;
  gPir = false; gTouch = false;
  lastWifiCheck = lastSensorRead = lastOledUpdate = 0;
  lastBuzzer = lastTouch = lastBreatheUpdate = 0;
  sensorFailCount = 0;
  sensorHealthy = true;
}
```

- Dòng 106–114: 5 **hook reset static** được **khai báo trước** ở đầu file, gói trong `#ifdef SMARTFARM_TEST_HOOKS`. Đây là các hàm do chính firmware cung cấp (định nghĩa trong `src/sensors.cpp`, `src/actuators.cpp`, `src/control.cpp`, `src/demo.cpp`, `src/wifi.cpp` — xem PHẦN 5–8, 11) và chỉ tồn tại khi build kèm `-DSMARTFARM_TEST_HOOKS` (cờ này do `run_tests.sh` luôn truyền — xem PHẦN 16). Firmware nạp cho kit thật không chứa đoạn này.
- Dòng 116: `resetState` = "đưa phần mềm về trạng thái xuất phát" trước mỗi test. Vì mọi test dùng chung biến **toàn cục** của firmware, nếu không reset thì kết quả test trước sẽ **rò rỉ** sang test sau, gây lỗi "ma".
- Dòng 117: `mockReset()` — xóa sạch toàn bộ thế giới mock: mức chân, giá trị ADC, tần số còi, đồng hồ `millis()` giả... coi như vừa cấp nguồn lại.
- Dòng 118–124: gọi lần lượt 5 hook vừa khai báo để dọn các biến `static` *bên trong* `src/` — thứ mà `mockReset()` không chạm tới: cửa sổ lọc trung bình của cảm biến, cờ còi, các cờ cảnh báo/cối xay gió/máng ăn và `sPrevTouch` (trong `control.cpp`), trạng thái trình diễn (trong `demo.cpp`), cờ `sWarned` (trong `wifi.cpp`). Nhờ vậy mỗi test bắt đầu từ đúng một điểm, không phụ thuộc thứ tự chạy.
- Dòng 125–126: trả 6 ngưỡng về mặc định (đất 400–700, nước 200, ánh sáng 500, nhiệt 25–30). Các test có thể đã từng đổi ngưỡng qua HTTP (`/threshold`) nên phải trả lại gốc.
- Dòng 127: bật lại 4 chế độ tự động: bơm, đèn, RGB, quạt (gán nối tiếp `a = b = c = ... = true`).
- Dòng 128: `breatheAuto = false` — đèn thở là tính năng "tùy chọn bật", mặc định tắt giống firmware thật.
- Dòng 129: tắt 4 trạng thái thiết bị đang chạy: relay (bơm), quạt, LED xanh, đèn thở.
- Dòng 130: tắt cối xay gió.
- Dòng 131: màu RGB về 0 (tắt màu).
- Dòng 132–133: độ sáng đèn thở về 0 và chiều tăng/giảm về 1 (tăng dần) — đèn thở hoạt động theo chu kỳ tăng/giảm độ sáng.
- Dòng 134: góc servo về 0 độ.
- Dòng 135: đặt số liệu cảm biến toàn cục về mức "an toàn ở giữa": nhiệt độ 22°C, độ ẩm 50%, ánh sáng 300, đất 500, nước 800 — đều không kích hoạt báo động, để mỗi test tự chọn kịch bản cần.
- Dòng 136: tắt công tắc chuyển động PIR và cảm ứng chạm.
- Dòng 137–138: đưa các **mốc thời gian** `millis()` về 0: lần kiểm tra WiFi, lần đọc cảm biến, lần cập nhật OLED, lần kêu còi, lần chạm, lần cập nhật đèn thở. Nhờ đó các bộ đếm throttle (giới hạn tần suất) trong firmware hoạt động lại ngay từ đầu thay vì tưởng "vừa chạy xong".
- Dòng 139: xóa bộ đếm số lần đọc cảm biến thất bại.
- Dòng 140: đánh dấu cảm biến khỏe mạnh trở lại.
- Tóm lại dòng 116–141: một nút "reset toàn bộ" — trả cả biến toàn cục lẫn biến static trong `src/` về mặc định, điểm khởi đầu công bằng cho mọi test.

```cpp
// Mô phỏng đúng phần điều khiển trong loop() của SmartFarm.ino (không có HTTP/OLED)
static void simLoop() {
  unsigned long now = mockMillis();
  gTouch = readTouch();
  gPir = readPIR();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    lastSensorRead = now;
  }
  if (gWater < (int)waterThresholdLow && relayState) setRelay(false);
  if (demoRunning()) {
    updateDemo();
    controlBreathe();
  } else {
    controlPump(); controlFan(); controlLight(); controlRgb(); controlBreathe();
    controlTouch(); controlAlerts(); controlWindmill(); controlFeed();
  }
}
```

- Dòng 143: comment nói rõ `simLoop` chép lại đúng phần điều khiển của `loop()` thật nhưng **bỏ** phần HTTP/OLED — dùng cho các test không quan tâm mạng và màn hình.
- Dòng 145: lấy đồng hồ giả `mockMillis()` (thay cho `millis()` thật) để đo thời gian mô phỏng.
- Dòng 146–147: đọc ngay cảm biến chạm và PIR mỗi vòng, đúng như firmware (hai cảm biến này không cần bộ lọc trung bình).
- Dòng 148–151: nếu đã qua `SENSOR_INTERVAL` kể từ lần đọc trước (`now - lastSensorRead >= SENSOR_INTERVAL`) thì gọi `readAllSensors()` — đọc cảm biến DHT/ADC có lọc trung bình — rồi chốt lại mốc thời gian. Đây là throttling: không đọc cảm biến quá dày.
- Dòng 152: bất biến an toàn: nếu mức nước thấp hơn ngưỡng (`gWater < (int)waterThresholdLow`, ép về `int` vì ngưỡng là `float`) **và** bơm đang bật thì tắt bơm ngay — chống bơm chạy khô.
- Dòng 153–155: nếu demo đang chạy, chỉ gọi `updateDemo()` và `controlBreathe()` — các bộ điều khiển tự động khác bị tạm khóa để không giành quyền điều khiển thiết bị với demo.
- Dòng 156–159: chế độ thường — gọi lần lượt toàn bộ các bộ điều khiển tự động đúng thứ tự như trong `loop()` thật: bơm, quạt, đèn, RGB, đèn thở, chạm, cảnh báo (còi), cối xay gió, cho ăn.

```cpp
static int invoke(const char *path,
                  const char *a1 = nullptr, const char *v1 = nullptr,
                  const char *a2 = nullptr, const char *v2 = nullptr,
                  const char *a3 = nullptr, const char *v3 = nullptr) {
  if (WEB_TOKEN[0] != '\0') mockServerSetHeader("X-Auth-Token", WEB_TOKEN);
  if (a1) mockServerSetArg(a1, v1);
  if (a2) mockServerSetArg(a2, v2);
  if (a3) mockServerSetArg(a3, v3);
  bool found = mockServerInvoke(path);
  CHECK(found);
  return mockServerLastCode();
}
```

- Dòng 162–165: hàm gọi một "request HTTP giả" tới firmware. Tham số: `path` là đường dẫn (ví dụ `/api`), rồi tối đa 3 cặp `(tên, giá trị)` tham số query dạng `?ten=giatri`. Các giá trị mặc định `= nullptr` nghĩa là **không truyền** nếu bỏ qua — nhờ vậy lúc gọi chỉ cần khai báo những tham số mình dùng.
- Dòng 166: nếu bản build có đặt `WEB_TOKEN` (đọc từ `config.h`), tự gắn header `X-Auth-Token` cho mọi request giả — nhờ đó các test hiện có chạy được cả trên bản build bật token mà không phải sửa từng lời gọi. Bản build không đặt token (mặc định) thì dòng này không làm gì.
- Dòng 167–169: với mỗi cặp có tên (`a1`, `a2`, `a3` khác null), nạp vào mock server để hàm xử lý `server.arg("ten")` đọc được y như trên thiết bị thật.
- Dòng 170: `mockServerInvoke(path)` gọi hàm xử lý web thật (handleApi, handleToggle...) tương ứng đường dẫn; trả về `true` nếu tìm thấy route, `false` nếu không.
- Dòng 171: `CHECK(found)` — nếu route không tồn tại thì test fail ngay lập tức, giúp phát hiện lỗi gõ sai đường dẫn.
- Dòng 172: trả về **mã HTTP** mà handler đã đặt (200 OK, 400 lỗi tham số...) để test so sánh bằng `CHECK_EQ(invoke(...), 200)`.

```cpp
static void runLoop(unsigned long simMs, unsigned long stepMs = 10) {
  unsigned long start = mockMillis();
  while (mockMillis() - start < simMs) {
    mockAdvance(stepMs);
    loop();
  }
}
```

- Dòng 175: chạy thử `loop()` thật trong một khoảng **thời gian mô phỏng** `simMs` (mili-giây), mỗi bước nhảy đồng hồ `stepMs` mili-giây (mặc định 10ms — xấp xỉ một vòng lặp thật).
- Dòng 176: ghi lại mốc bắt đầu.
- Dòng 177: lặp chừng nào thời gian mô phỏng trôi qua chưa đủ `simMs`.
- Dòng 178–179: đẩy đồng hồ giả thêm `stepMs` (`mockAdvance`) rồi gọi `loop()` thật đúng một lần.
- Ý nghĩa: nhờ đồng hồ giả, "chạy 1 giờ" chỉ tốn vài giây thực, và kết quả luôn tái lập được (không phụ thuộc tốc độ máy).

```cpp
static void primeSensors(int water) {
  mockDhtTemp = 26.0f; mockDhtHum = 60.0f;
  mockAdc[LDR_PIN] = 1000; mockAdc[SOIL_PIN] = 600;
  mockAdc[WATER_PIN] = INVERT_WATER ? 4095 - water : water;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  lastSensorRead = mockMillis();
}
```

- Dòng 183: `primeSensors` = "mồi" cảm biến trước khi test chạy, để bộ lọc trung bình trượt có sẵn dữ liệu ổn định ngay từ những vòng đầu. Tham số `water` là giá trị mức nước muốn giả lập.
- Dòng 184: đặt DHT11 giả: 26.0°C, độ ẩm 60%. Hậu tố `f` nghĩa là số thực (`float`).
- Dòng 185: đặt ADC giả: ánh sáng (LDR) = 1000, độ ẩm đất = 600.
- Dòng 186: đặt ADC nước. Nếu `INVERT_WATER` bật (cảm biến nước kiểu đảo: nước càng nhiều trị càng nhỏ) thì lấy `4095 - water` để đảo ngược; ngược lại dùng thẳng `water`. 4095 là giá trị ADC 12 bit lớn nhất của ESP32.
- Dòng 187: gọi `readAllSensors()` đúng `SENSOR_FILTER_SIZE` lần — số phần tử của bộ lọc trung bình — để bộ lọc đầy toàn giá trị mới. Nếu chỉ gọi 1 lần, giá trị trung bình vẫn lẫn dữ liệu cũ/0 và test sẽ đọc ra số sai lệch.
- Dòng 188: chốt `lastSensorRead = mockMillis()` — coi như vừa đọc cảm biến xong, để `simLoop` không đọc lại ngay lập tức mà chờ đủ chu kỳ.

#### Cách đọc một test

Các phần tiếp theo (14.2–14.12) mô tả từng hàm test, nhưng chỉ cần nắm **4 macro**: `CHECK` (điều kiện đúng), `CHECK_EQ` (hai số bằng nhau), `CHECK_NEAR` (hai số thực gần nhau), `CHECK_STR` (hai chuỗi giống nhau). Mọi test đều theo cùng một mô-típ 3 bước: **chuẩn bị** — gọi `resetState()` rồi chỉnh các biến mock (`mockAdc`, `mockDhtTemp`, `mockPinLevel`...) hoặc gọi `invoke`/`primeSensors`; **gọi hàm thật** — hàm điều khiển thật, `simLoop()`, `runLoop()` hay HTTP handler thật; **kiểm tra kết quả** — một loạt `CHECK*` so kết quả với mong đợi. Chỉ dòng nào in `FAIL` mới là có lỗi; `main()` cuối cùng tổng kết `gChecks`/`gFails` để báo PASS/FAIL toàn bộ.

### 14.2. T1–T12 — unit test cảm biến, bơm, quạt, đèn, chạm, RGB, còi, cơ cấu (dòng 195–476)

Mười hai test dưới đây (T1–T12) gọi trực tiếp các hàm điều khiển **thật** trong `src/sensors.cpp`, `src/control.cpp`, `src/actuators.cpp` rồi kiểm tra kết quả qua các biến mock phần cứng (`test/mocks/mock.h`). Mỗi test bắt đầu bằng `resetState()` (dòng 116–141 của `test_main.cpp`) để trả mọi ngưỡng, cờ chế độ, ngân hàng ADC/millis và cả các biến static trong `src/` (qua 5 hook reset) về mặc định, nhờ đó các test độc lập với nhau. Bốn macro `CHECK`, `CHECK_EQ`, `CHECK_NEAR`, `CHECK_STR` đã được giải thích ở mục 14.1 — ở đây chỉ nói ngắn gọn mỗi dòng kiểm tra điều gì.

#### T1 — testSensorsAverage() (dòng 196–218)

**Mục đích:** Kiểm tra bộ lọc trung bình động (moving average) 5 mẫu của cảm biến ánh sáng/đất/nước, nhiệt độ, độ ẩm, và hướng chuẩn hoá tín hiệu theo cờ `INVERT_*`. Test phải chạy đúng cho cả board thường lẫn board đọc ngược.

```cpp
static void testSensorsAverage() {
  printf("T1  sensors: moving average + direction flags\n");
  resetState();
  mockDhtTemp = 26.5f; mockDhtHum = 61.0f;
  mockAdc[LDR_PIN] = 3000; mockAdc[SOIL_PIN] = 600; mockAdc[WATER_PIN] = 900;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();

  auto dir = [](int raw, int invert) { return invert ? 4095 - raw : raw; };
  CHECK_EQ(gLight, dir(3000, INVERT_LIGHT));
  CHECK_EQ(gSoil, dir(600, INVERT_SOIL));
  CHECK_EQ(gWater, dir(900, INVERT_WATER));
  CHECK_NEAR(gTemp, 26.5, 0.05);
  CHECK_NEAR(gHum, 61.0, 0.05);

  mockAdc[LDR_PIN] = 2000;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_EQ(gLight, dir(2000, INVERT_LIGHT));

  mockAdc[LDR_PIN] = 1000;
  readAllSensors();
  int expected = (4 * dir(2000, INVERT_LIGHT) + dir(1000, INVERT_LIGHT)) / 5;
  CHECK_EQ(gLight, expected);
}
```

- Dòng 197: in tên test ra console — bộ test dùng `printf` thủ công (không dùng framework), mỗi test tự in mã T1…T14.
- Dòng 198: `resetState()` xoá toàn bộ mock (ADC = 0, millis = 0) và trả ngưỡng/chế độ auto về mặc định.
- Dòng 199: nạp DHT11 giả 26.5 °C và 61 %RH.
- Dòng 200: nạp ADC giả: LDR = 3000 (sáng mạnh), đất = 600, nước = 900. Với build thường (`INVERT_*` = 0), số càng lớn nghĩa là càng sáng / càng ẩm / càng đầy nước.
- Dòng 201: gọi `readAllSensors()` (hàm thật trong `src/sensors.cpp:41`) đúng `SENSOR_FILTER_SIZE` = 5 lần → cửa sổ trung bình đầy, 5 mẫu giống nhau nên trung bình bằng chính giá trị nạp.
- Dòng 203: lambda `dir(raw, invert)` mô phỏng `normalizeAdc()` của firmware (`src/sensors.cpp:31-33`): nếu cờ `INVERT_*` = 1 thì đảo thành `4095 - raw`. Nhờ đó một test chạy đúng cho cả build `core3` thường lẫn build `core2-invert`.
- Dòng 204–206: kiểm tra `gLight`/`gSoil`/`gWater` khớp hướng chuẩn hoá. Build thường kỳ vọng 3000/600/900; build invert kỳ vọng 1095/3495/3195.
- Dòng 207–208: kiểm tra nhiệt độ/độ ẩm trung bình đúng 26.5/61.0 với dung sai 0.05.
- Dòng 210–212: đổi LDR = 2000 rồi nạp đầy cửa sổ 5 mẫu mới → giá trị lọc cập nhật thành 2000 (cửa sổ bị thay hoàn toàn).
- Dòng 214–215: đặt LDR = 1000 nhưng chỉ gọi `readAllSensors()` 1 lần → cửa sổ vòng giờ giữ 4 mẫu 2000 cũ + 1 mẫu 1000 mới.
- Dòng 216: công thức trung bình kỳ vọng `(4 * 2000 + 1000) / 5 = 1800` (build thường); build invert là `(4 * 2095 + 3095) / 5 = 2295`.
- Dòng 217: khẳng định `gLight` đúng bằng trung bình động — chứng minh cửa sổ thay dần từng mẫu chứ không nhảy ngay theo giá trị mới (chống nhiễu).

#### T2 — testDhtFailover() (dòng 221–246)

**Mục đích:** Kiểm tra cơ chế chịu lỗi khi DHT11 trả về `NAN` (lỗi checksum/không giao tiếp): dùng lại giá trị tốt cuối cùng, và chỉ đánh dấu cảm biến hỏng sau nhiều lần lỗi liên tiếp.

```cpp
static void testDhtFailover() {
  printf("T2  sensors: DHT11 NaN failover\n");
  resetState();
  mockDhtTemp = 26.0f; mockDhtHum = 55.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK_NEAR(gTemp, 26.0, 0.05);

  mockDhtTemp = NAN;
  readAllSensors();
  CHECK_NEAR(gTemp, 26.0, 0.05);
  for (int i = 0; i < 6; i++) readAllSensors();
  CHECK(!sensorHealthy);

  mockDhtTemp = 24.0f; mockDhtHum = 50.0f;
  for (int i = 0; i < SENSOR_FILTER_SIZE; i++) readAllSensors();
  CHECK(sensorHealthy);
  CHECK_NEAR(gTemp, 24.0, 0.05);

  // Độ ẩm NaN dùng lại giá trị lọc tốt cuối cùng (giống nhiệt độ)
  mockDhtTemp = 27.0f; mockDhtHum = NAN;
  readAllSensors();
  CHECK_NEAR(gHum, 50.0, 0.5);
  CHECK(sensorHealthy);        // một NaN độ ẩm đơn lẻ không đánh dấu cảm biến lỗi
  mockDhtHum = 50.0f;
  readAllSensors();
}
```

- Dòng 222–225: nạp DHT giả 26.0 °C / 55 %RH rồi đọc đủ 5 lần cho cửa sổ đầy.
- Dòng 226: xác nhận nhiệt độ lọc đúng 26.0 khi DHT hoạt động bình thường.
- Dòng 228–229: cho DHT trả `NAN` (số "không phải số" — dấu hiệu đọc lỗi) rồi đọc 1 lần.
- Dòng 230: nhiệt độ vẫn 26.0 — firmware thay NaN bằng giá trị tốt cuối cùng trong bộ đệm (`src/sensors.cpp:50-56`), nên một lần lỗi không kéo giá trị xuống.
- Dòng 231–232: đọc lỗi thêm 6 lần → bộ đếm `sensorFailCount` vượt mốc 5 → firmware gắn cờ `sensorHealthy = false` (báo cảm biến hỏng). Test kiểm tra cờ này.
- Dòng 234–236: DHT hồi phục (24.0 °C / 50 %RH), đọc đủ 5 mẫu → cờ `sensorHealthy` tự bật lại `true` khi đọc thành công.
- Dòng 237: nhiệt độ ra đúng 24.0 — giá trị lỗi cũ đã bị đẩy ra khỏi cửa sổ trung bình.
- Dòng 239–242: chỉ độ ẩm `NAN`: firmware dùng lại 50 % tốt cuối (`src/sensors.cpp:57-59`) nên `gHum` ≈ 50 (dung sai 0.5).
- Dòng 243: một NaN độ ẩm đơn lẻ **không** đánh dấu cảm biến hỏng — chỉ nhiệt độ lỗi tích luỹ mới tính vào `sensorFailCount`.
- Dòng 244–245: trả DHT về bình thường và đọc thêm 1 lần để dọn trạng thái sạch cho test sau.

#### T3 — testPumpHysteresis() (dòng 249–273)

**Mục đích:** Kiểm tra điều khiển bơm theo độ ẩm đất có trễ (hysteresis: bật dưới ngưỡng thấp, tắt trên ngưỡng cao — vùng giữa giữ nguyên trạng thái), và chống cấu hình ngưỡng bị đảo gây nhấp nháy relay.

```cpp
static void testPumpHysteresis() {
  printf("T3  pump: soil hysteresis\n");
  resetState();
  gWater = 900;
  gSoil = 300;
  controlPump();
  CHECK(relayState);
  checkRelayPin(true);
  gSoil = 500;                 // giữa các ngưỡng: không đổi
  controlPump();
  CHECK(relayState);
  gSoil = 800;
  controlPump();
  CHECK(!relayState);
  checkRelayPin(false);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  soilThresholdLow = 700; soilThresholdHigh = 400;
  gSoil = 500; gWater = 900;
  controlPump();
  CHECK(!relayState);
  controlPump();
  CHECK(!relayState);
}
```

- Dòng 252–254: nước đủ (900 > ngưỡng 200), đất khô 300 < ngưỡng thấp 400 → `controlPump()` bật bơm (nhánh `src/control.cpp:32-34`).
- Dòng 255: xác nhận cờ `relayState` = bật.
- Dòng 256: `checkRelayPin(true)` (helper dòng 89–95) kiểm tra mức điện thật trên chân relay khớp cực tính của build — active-high mong `HIGH`, build `passive-lowrelay` mong `LOW`.
- Dòng 257–259: đất 500 nằm giữa 400 và 700 (vùng trễ) → `controlPump()` không đổi gì, bơm vẫn chạy. Đây là mục đích chính của hysteresis: chống bật/tắt liên tục khi giá trị dao động quanh một ngưỡng.
- Dòng 260–262: đất 800 > ngưỡng cao 700 → tắt bơm.
- Dòng 263: xác nhận chân relay đã xuống mức tắt.
- Dòng 265–268: cố tình đặt ngưỡng bị đảo (`soilThresholdLow` = 700 > `soilThresholdHigh` = 400) — cấu hình sai nếu không chặn sẽ làm relay đóng/ngắt vô hạn.
- Dòng 269–272: `controlPump()` phát hiện `soilThresholdHigh <= soilThresholdLow` (`src/control.cpp:29`) và thoát ngay, nên qua 2 lần gọi bơm vẫn giữ tắt. Khẳng định bất biến an toàn.

#### T4 — testPumpLowWaterSafety() (dòng 276–291)

**Mục đích:** Kiểm tra an toàn hết nước: nước dưới ngưỡng thì bơm **luôn** bị tắt, kể cả khi người dùng đang bật bơm thủ công — chạy bơm khô sẽ hỏng bơm.

```cpp
static void testPumpLowWaterSafety() {
  printf("T4  pump: low-water safety\n");
  resetState();
  pumpAuto = false;
  setRelay(true);
  CHECK(relayState);
  gWater = 100;
  controlPump();
  CHECK(!relayState);

  pumpAuto = true;
  gSoil = 100;                 // rất khô nhưng không có nước
  gWater = 100;
  controlPump();
  CHECK(!relayState);
}
```

- Dòng 278–281: `resetState()` rồi tắt chế độ tự động (`pumpAuto = false`) và bật bơm bằng tay `setRelay(true)` — xác nhận bơm đang chạy.
- Dòng 282–284: hạ mức nước xuống 100 < ngưỡng 200 → `controlPump()` ưu tiên an toàn, tắt bơm ngay cả trong chế độ thủ công (`src/control.cpp:18-24`).
- Dòng 286–290: bật lại auto, đất rất khô (100 < 400) đáng lẽ phải bật bơm, nhưng nước vẫn 100 → vẫn bắt buộc tắt. Qua đó thấy hai lớp bảo vệ: chế độ thủ công và nhu cầu tưới đều phải nhường điều kiện "có nước".

#### T5 — testFanHysteresis() (dòng 294–309)

**Mục đích:** Kiểm tra quạt chạy theo nhiệt độ có trễ (bật trên ngưỡng cao, tắt dưới ngưỡng thấp) và bỏ qua khi ngưỡng bị đảo.

```cpp
static void testFanHysteresis() {
  printf("T5  fan: temperature hysteresis\n");
  resetState();
  gTemp = 31; controlFan(); CHECK(fanState);
  gTemp = 27; controlFan(); CHECK(fanState);
  gTemp = 24; controlFan(); CHECK(!fanState);

  // Ngưỡng đảo không được làm relay bật/tắt liên tục
  resetState();
  tempThresholdHigh = 25; tempThresholdLow = 30;
  gTemp = 27;
  controlFan();
  CHECK(!fanState);
  controlFan();
  CHECK(!fanState);
}
```

- Dòng 297: 31 °C > ngưỡng cao 30 → `controlFan()` bật quạt (`src/control.cpp:48-49`).
- Dòng 298: 27 °C nằm giữa 25–30 (vùng trễ) → quạt giữ nguyên đang bật, không tắt vội.
- Dòng 299: 24 °C < ngưỡng thấp 25 → tắt quạt.
- Dòng 301–303: `resetState()` rồi đảo ngưỡng (`tempThresholdHigh` = 25 < `tempThresholdLow` = 30) — cấu hình sai.
- Dòng 304–308: `controlFan()` thoát sớm vì `tempThresholdHigh <= tempThresholdLow` (`src/control.cpp:46`) → quạt vẫn tắt qua 2 lần gọi, chống dao động relay.

#### T6 — testLightPirAlerts() (dòng 312–332)

**Mục đích:** Kiểm tra đèn xanh dương tự động theo ánh sáng (ngưỡng ±50), quy tắc PIR được ưu tiên hơn đèn auto, và cảnh báo chuyển động bật/tắt đèn đúng chủ sở hữu.

```cpp
static void testLightPirAlerts() {
  printf("T6  light auto / PIR / alerts\n");
  resetState();
  gLight = 400;                // tối
  controlLight();
  CHECK(blueLedState);
  gLight = 600;                // sáng
  controlLight();
  CHECK(!blueLedState);

  gPir = true;                 // PIR nắm LED: đèn auto phải nhường
  gLight = 400;
  controlLight();
  CHECK(!blueLedState);

  controlAlerts();             // PIR bật đèn
  CHECK(blueLedState);
  gPir = false;
  controlAlerts();             // và tắt khi hết chuyển động
  CHECK(!blueLedState);
}
```

- Dòng 314–317: `resetState()` đặt `lightThreshold` = 500; ánh sáng 400 < 450 (= 500 − 50) → `controlLight()` bật LED (`src/control.cpp:60-61`).
- Dòng 318–320: ánh sáng 600 > 550 (= 500 + 50) → tắt LED. Hai ngưỡng cách nhau ±50 tạo vùng trễ quanh `lightThreshold`.
- Dòng 322–325: đặt `gPir = true` (có người chuyển động) → `controlLight()` bỏ qua hoàn toàn (`src/control.cpp:59`) để nhường quyền cho PIR; dù trời tối, đèn auto vẫn không bật ở bước này.
- Dòng 327–328: `controlAlerts()` là nơi PIR thực sự bật đèn; cờ `sPirLitLed` ghi nhớ "LED này do PIR bật" → đèn sáng.
- Dòng 329–331: hết chuyển động (`gPir = false`) → `controlAlerts()` chỉ tắt đèn nếu chính PIR là thứ đã bật nó (không tắt đèn do người dùng/auto bật). LED tắt.

#### T7 — testBreatheAndLedModes() (dòng 335–363)

**Mục đích:** Kiểm tra hiệu ứng "thở" của LED (PWM tăng/giảm mượt mỗi 30 ms) và quy tắc phân xử giữa chế độ thở với lệnh sáng đều; đồng thời kiểm tra đường ghi PWM trực tiếp.

```cpp
static void testBreatheAndLedModes() {
  printf("T7  LED: steady/breathe modes + PWM dispatch\n");
  resetState();
  setBreatheLed(true);
  CHECK(breatheLedState);
  CHECK(!blueLedState);
  CHECK_EQ(currentLedPwm(), 0);

  uint32_t first = 0, peak = 0;
  for (int i = 0; i < 400; i++) {
    mockAdvance(BREATHE_INTERVAL);
    controlBreathe();
    uint32_t p = currentLedPwm();
    if (p && !first) first = p;
    if (p > peak) peak = p;
  }
  CHECK_EQ(first, 5);
  CHECK_EQ(peak, 255);

  setBlueLed(true);            // lệnh sáng đều thắng breathing
  CHECK(!breatheLedState);
  CHECK(blueLedState);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);

  setLedPwm(123);
  CHECK_EQ(currentLedPwm(), 123);
}
```

- Dòng 337–339: `resetState()` rồi `setBreatheLed(true)` → cờ `breatheLedState` bật.
- Dòng 340: chế độ thở và đèn sáng liên tục loại trừ nhau → `blueLedState` phải `false`.
- Dòng 341: `currentLedPwm()` (helper dòng 81–87) đọc giá trị PWM cuối cùng đã ghi vào mock; vừa bật thở độ sáng còn 0 nên PWM = 0 (chưa ghi PWM ngay).
- Dòng 343–350: vòng lặp 400 lần: mỗi lần tua đồng hồ giả `mockAdvance(BREATHE_INTERVAL)` = 30 ms rồi gọi `controlBreathe()`. Firmware tăng/giảm độ sáng 5 đơn vị mỗi 30 ms (`src/control.cpp:97`). Biến `first` lưu giá trị PWM khác 0 đầu tiên, `peak` lưu giá trị lớn nhất.
- Dòng 351: bước sáng đầu tiên phải là 5 → xác nhận tốc độ tăng PWM đúng 5 mỗi nhịp 30 ms.
- Dòng 352: đỉnh phải chạm 255 → xác nhận chu kỳ thở lên tới trần PWM 8 bit rồi mới đảo chiều.
- Dòng 354–357: `setBlueLed(true)` trong lúc đang thở: lệnh sáng đều "thắng", tắt cờ thở và ghi PWM 255 ngay (`src/actuators.cpp:43-49`).
- Dòng 358–359: tắt đèn sáng đều → PWM về 0.
- Dòng 361–362: `setLedPwm(123)` ghi thẳng giá trị PWM vào phần cứng; đọc lại đúng 123 → xác nhận đường điều khiển PWM không bị kẹp hay nhầm giá trị.

#### T8 — testTouchToggle() (dòng 366–388)

**Mục đích:** Kiểm tra nút chạm đảo trạng thái LED theo sườn nhấn (không lặp khi giữ), có chống dội (debounce) 300 ms, và thao tác thủ công tự tắt chế độ auto.

```cpp
static void testTouchToggle() {
  printf("T8  touch: edge toggle + debounce\n");
  resetState();
  mockAdvance(5000);
  gTouch = true;
  controlTouch();
  CHECK(blueLedState);
  controlTouch();              // giữ: không lặp lại
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();
  gTouch = true;
  mockAdvance(TOUCH_DEBOUNCE - 100);
  controlTouch();              // trong thời gian debounce: lần nhấn bị nuốt
  CHECK(blueLedState);
  gTouch = false;
  controlTouch();              // nhả
  mockAdvance(200);            // lúc này đã qua cửa sổ debounce
  gTouch = true;
  controlTouch();              // tắt
  CHECK(!blueLedState);
  CHECK(!lightAuto);           // thao tác thủ công đã tắt auto
}
```

- Dòng 369: tua đồng hồ giả 5000 ms để bộ đếm chống dội `lastTouch` (0) chắc chắn đã hết hạn.
- Dòng 370–372: giả lập nhấn chạm (`gTouch = true`) → `controlTouch()` nhận sườn lên, khoảng cách > 300 ms nên đảo LED (bật) và tắt `lightAuto` (thao tác tay nhường chế độ auto, `src/control.cpp:110-124`).
- Dòng 373–374: vẫn giữ nhấn và gọi lại → không có sườn mới nên LED không đảo lần hai (chống lặp khi giữ tay).
- Dòng 375–376: nhả tay — firmware ghi nhớ trạng thái trước đó là "đang nhấn".
- Dòng 377–380: nhấn lại chỉ sau 200 ms (< `TOUCH_DEBOUNCE` 300 ms) → lần nhấn bị "nuốt" (bỏ qua), LED vẫn sáng.
- Dòng 381–383: nhả lần nữa rồi tua thêm 200 ms → tổng từ lần nhấn hợp lệ trước đã quá 300 ms.
- Dòng 384–386: nhấn hợp lệ → đảo LED lần hai (tắt).
- Dòng 387: `lightAuto` đã bị tắt từ lần nhấn đầu và giữ nguyên `false`.

#### T9 — testRgbPlant() (dòng 391–414)

**Mục đích:** Kiểm tra đèn RGB trồng cây (LED chẵn đỏ, LED lẻ xanh dương) bật khi trời tối, tắt khi trời sáng, và màu thủ công không bị "kẹt" khi chế độ auto bật lại.

```cpp
static void testRgbPlant() {
  printf("T9  RGB: plant pattern (activity 9)\n");
  resetState();
  setRgb(10, 20, 30);
  CHECK_EQ(mockPixel[3], (10 << 16) | (20 << 8) | 30);

  gLight = 400;                // tối -> kiểu đèn cho cây
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
  CHECK_EQ(mockPixel[1], 0x0000FF);
  CHECK_EQ(mockPixel[2], 0xFF0000);
  CHECK_EQ(mockPixel[3], 0x0000FF);
  CHECK_EQ(rgbR, 255); CHECK_EQ(rgbG, 0); CHECK_EQ(rgbB, 255);

  gLight = 600;                // sáng -> tắt
  controlRgb();
  for (int i = 0; i < RGB_COUNT; i++) CHECK_EQ(mockPixel[i], 0);
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbB, 0);

  setRgb(10, 20, 30);          // màu thủ công, rồi tối lại
  gLight = 400;
  controlRgb();
  CHECK_EQ(mockPixel[0], 0xFF0000);
}
```

- Dòng 393–395: `setRgb(10, 20, 30)` ghi màu cho cả 4 LED NeoPixel; kiểm tra LED thứ 4 (index 3, tương ứng `RGB_COUNT` = 4) mang đúng màu gói `(R << 16) | (G << 8) | B` = 0x0A141E.
- Dòng 397–398: trời tối 400 < 450 → `controlRgb()` bật kiểu đèn trồng cây (`src/control.cpp:76-77`).
- Dòng 399–402: pattern cây: LED index chẵn (0, 2) đỏ `0xFF0000`, index lẻ (1, 3) xanh dương `0x0000FF` (`src/actuators.cpp:93-101`).
- Dòng 403: `rgbR` = 255, `rgbG` = 0, `rgbB` = 255 — "chữ ký" của pattern cây, để `controlRgb()` nhận biết bằng cách so sánh (`src/control.cpp:74`) thay vì lưu cờ riêng.
- Dòng 405–407: trời sáng 600 > 550 → `controlRgb()` tắt toàn bộ 4 LED (màu 0).
- Dòng 408: hai biến trạng thái RGB về 0 tương ứng.
- Dòng 410–413: đặt màu thủ công (10, 20, 30) rồi cho trời tối lại → `controlRgb()` thấy chưa phải pattern cây (`!plantOn`) nên ghi đè bằng pattern đèn trồng cây — màu thủ công không bị bỏ quên khỏi chế độ auto.

#### T10 — testBuzzerAlarm() (dòng 417–427)

**Mục đích:** Kiểm tra còi báo động kêu khi nước dưới ngưỡng và tắt khi mức nước hồi phục.

```cpp
static void testBuzzerAlarm() {
  printf("T10 buzzer: low-water alarm\n");
  resetState();
  checkBuzzer(false);
  gWater = 100;
  controlAlerts();
  checkBuzzer(true);
  gWater = 800;
  controlAlerts();
  checkBuzzer(false);
}
```

- Dòng 419–420: `resetState()` (nước mặc định 800) rồi `checkBuzzer(false)` — helper dòng 97–104 xác nhận còi đang tắt: build buzzer chủ động kiểm mức chân `BUZZER_PIN`, build buzzer thụ động kiểm tần số tone = 0.
- Dòng 421–423: nước 100 < ngưỡng 200 → `controlAlerts()` bật còi cảnh báo hết nước (`src/control.cpp:128-134`); cảnh báo nước có ưu tiên trước cảnh báo chuyển động.
- Dòng 424–426: nâng nước lên 800 → nhánh "đã hết cảnh báo" tắt còi. Cơ chế cạnh lên/xuống chỉ log một lần, không kêu/spam liên tục.

#### T11 — testWindmillSweep() (dòng 430–448)

**Mục đích:** Kiểm tra servo cối xay gió quét qua lại trong biên cho phép (không vượt giới hạn cơ khí) và trở về góc nghỉ khi tắt.

```cpp
static void testWindmillSweep() {
  printf("T11 windmill: servo sweep\n");
  resetState();
  setWindmill(true);
  CHECK(windmillOn);
  int minA = 999, maxA = -1;
  for (int i = 0; i < 120; i++) {
    mockAdvance(WINDMILL_INTERVAL);
    controlWindmill();
    if (mockServoAngle < minA) minA = mockServoAngle;
    if (mockServoAngle > maxA) maxA = mockServoAngle;
  }
  CHECK(minA >= WINDMILL_MIN_ANGLE);
  CHECK(maxA <= WINDMILL_MAX_ANGLE);
  CHECK_EQ(maxA, WINDMILL_MAX_ANGLE);
  CHECK(minA < maxA);
  setWindmill(false);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
}
```

- Dòng 432–434: `resetState()` rồi `setWindmill(true)` → cờ `windmillOn` bật (`src/control.cpp:165-174`).
- Dòng 435: khởi tạo `minA`/`maxA` bằng giá trị "giả" (999/−1) để vòng lặp tự cập nhật theo góc thật.
- Dòng 436–440: 120 lần, mỗi lần tua 30 ms rồi gọi `controlWindmill()`; mỗi bước servo quét 6° (`src/control.cpp:181`). Test ghi lại góc nhỏ nhất và lớn nhất từng thấy trên mock.
- Dòng 442–443: góc không vượt biên hai phía nhờ clamp trong firmware (`src/control.cpp:182-183`) → cơ cấu không bị kẹt hoặc vượt giới hạn cơ khí.
- Dòng 444: phải chạm đúng biên trên 180 → xác nhận quét hết biên độ thiết kế.
- Dòng 445: `minA < maxA` → servo thật sự quét qua lại chứ không đứng yên một góc.
- Dòng 446–447: tắt cối xay gió → servo được đưa về góc nghỉ `WINDMILL_MIN_ANGLE` = 0 (`src/control.cpp:171`).

#### T12 — testFeedGate() (dòng 451–474)

**Mục đích:** Kiểm tra cửa máng ăn mở đúng `FEED_DURATION` (2000 ms) rồi tự đóng, và phân xử servo dùng chung giữa cửa cho ăn với cối xay gió.

```cpp
static void testFeedGate() {
  printf("T12 feed: gate timing + shared servo\n");
  resetState();
  startFeed();
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  mockAdvance(FEED_DURATION - 1);
  controlFeed();
  CHECK(feedRunning());
  mockAdvance(1);
  controlFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  setWindmill(true);
  startFeed();                 // cho ăn chiếm servo dùng chung
  CHECK(feedRunning());
  CHECK(!windmillOn);
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);
  setWindmill(true);           // cối xay gió lấy lại
  CHECK(!feedRunning());
  CHECK(windmillOn);
  setWindmill(false);
}
```

- Dòng 453–456: `startFeed()` mở cửa: cờ feed bật, servo tới `FEED_OPEN_ANGLE` = 90° ngay lập tức.
- Dòng 457–459: tua 1999 ms (`FEED_DURATION − 1`) rồi `controlFeed()` → cửa còn mở. Đây là kiểm tra biên dưới: chưa đủ 2000 ms thì chưa được đóng.
- Dòng 460–463: tua thêm đúng 1 ms → tổng 2000 ms = `FEED_DURATION` → `controlFeed()` đóng cửa, servo về `FEED_CLOSE_ANGLE` = 0° (`src/control.cpp:205-208`). Kiểm tra đúng mốc thời gian.
- Dòng 465–469: bật cối xay gió rồi `startFeed()` — hai cơ cấu dùng chung một servo; `startFeed()` gọi `setWindmill(false)` (`src/control.cpp:192`) → cối xay gió nhường (`windmillOn` = false), cửa mở (servo 90°).
- Dòng 470–472: bật lại cối xay gió → `setWindmill(true)` gọi `stopFeed()` (`src/control.cpp:169`) → cửa đóng về mặt logic ngay (không cần chờ hết 2 giây) và cối xay gió lấy lại quyền điều khiển servo.
- Dòng 473: tắt cối xay gió để trả trạng thái sạch cho test kế tiếp.

### 14.3. T13–T15 và D1–D3 — thời tiết, settings, trình diễn, biên, tính bất biến, mốc thời gian (dòng 477–726)

Sáu test trong mục này dùng lại các tiện ích chung đã giới thiệu ở 14.1: `resetState()` (gọi `mockReset()` rồi đưa toàn bộ biến toàn cục về mặc định), `mockAdvance(ms)` (đẩy đồng hồ ảo `millis()` thêm `ms`), `primeSensors()`, `simLoop()`, `checkRelayPin()`/`checkBuzzer()` và `currentLedPwm()`. Bốn macro `CHECK` đã được giải thích ở 14.1 nên không nhắc lại. Các hằng số thời gian lấy từ `src/config.cpp`: `TOUCH_DEBOUNCE = 300`, `BREATHE_INTERVAL = 30`, `WINDMILL_INTERVAL = 30`, `FEED_DURATION = 2000` (ms); các ngưỡng mặc định: `soilThresholdLow = 400`, `soilThresholdHigh = 700`, `waterThresholdLow = 200`, `lightThreshold = 500`, `tempThresholdHigh = 30`, `tempThresholdLow = 25`.

#### T13 — testWeather() (chú thích dòng 476; thân hàm 458–466)

**Mục đích:** Kiểm tra hàm đoán thời tiết `weatherStatus()` (hoạt động 11) phối hợp ánh sáng `gLight` và độ ẩm `gHum` để trả về đúng một trong bốn nhãn `WEATHER_NORMAL/SUNNY/CLOUDY/RAIN`.

```cpp
// T13: đoán thời tiết (hoạt động 11)
static void testWeather() {
  printf("T13 weather: light + humidity guess\n");
  resetState();
  gLight = 800; gHum = 50; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gHum = 90;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 200; gHum = 90; CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gHum = 50;               CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gLight = 500;            CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
}
```

- Dòng 477: khai báo hàm test.
- Dòng 478: in tên test ra màn hình.
- Dòng 479: `resetState()` đưa mọi thứ về mặc định; riêng ở đây cần nhớ `lightThreshold = 500` vì `weatherStatus()` (sensors.cpp:93) so `gLight` với hai mốc 500 ± 50: "sáng" = `gLight > 550`, "tối" = `gLight < 450`, "ẩm" = `gHum > 80.0`.
- Dòng 480: `gLight = 800` (sáng, vì 800 > 550) và `gHum = 50` (không ẩm) → `WEATHER_SUNNY` (enum = 1). `CHECK_EQ` so số nguyên enum nên nếu sai sẽ in ra giá trị số 0..3.
- Dòng 481: giữ nguyên độ sáng, đổi `gHum = 90` (> 80, ẩm) → `WEATHER_CLOUDY` (2). Một mình độ ẩm đổi cũng đủ đổi nhãn.
- Dòng 482: `gLight = 200` (tối, vì 200 < 450), vẫn ẩm → `WEATHER_RAIN` (3).
- Dòng 483: `gHum = 50` hết ẩm, vẫn tối → `WEATHER_CLOUDY` (2).
- Dòng 484: `gLight = 500` rơi vào dải chết 450..550 (không sáng, không tối) → `WEATHER_NORMAL` (0), bất kể độ ẩm (lúc này `gHum = 50`).
- Dòng 485: đóng hàm.

#### T14 — testSettingsRoundtrip() (chú thích dòng 487; thân hàm 469–489)

**Mục đích:** Chứng minh `saveSettings()` rồi `loadSettings()` khôi phục đủ 6 ngưỡng số và 5 cờ chế độ tự động đúng như đã lưu — tức một "vòng tròn" lưu/đọc qua flash giả hoạt động hoàn hảo.

```cpp
// T14: lưu/đọc settings vòng tròn
static void testSettingsRoundtrip() {
  printf("T14 settings: save/load roundtrip\n");
  resetState();
  soilThresholdLow = 111; soilThresholdHigh = 222; waterThresholdLow = 133;
  lightThreshold = 654; tempThresholdHigh = 35.5f; tempThresholdLow = 21.5f;
  pumpAuto = false; lightAuto = false; rgbAuto = true; fanAuto = false; breatheAuto = true;
  saveSettings();

  soilThresholdLow = 0; soilThresholdHigh = 0; waterThresholdLow = 0;
  lightThreshold = 0; tempThresholdHigh = 0; tempThresholdLow = 0;
  pumpAuto = true; lightAuto = true; rgbAuto = false; fanAuto = true; breatheAuto = false;
  loadSettings();

  CHECK_NEAR(soilThresholdLow, 111, 0.01);
  CHECK_NEAR(soilThresholdHigh, 222, 0.01);
  CHECK_NEAR(waterThresholdLow, 133, 0.01);
  CHECK_EQ(lightThreshold, 654);
  CHECK_NEAR(tempThresholdHigh, 35.5, 0.01);
  CHECK_NEAR(tempThresholdLow, 21.5, 0.01);
  CHECK(!pumpAuto); CHECK(!lightAuto); CHECK(rgbAuto); CHECK(!fanAuto); CHECK(breatheAuto);
}
```

- Dòng 488: khai báo hàm test.
- Dòng 489: in tên test.
- Dòng 490: `resetState()` gọi `mockReset()`, trong đó có `memset(sPrefs, ...)` — xoá sạch vùng "flash" giả của mock Preferences, nên test bắt đầu từ trạng thái trắng.
- Dòng 491–492: gán bộ giá trị cố ý khác hẳn mặc định (111/222/133/654/35.5/21.5 so với 400/700/200/500/30/25). Nhờ vậy, nếu `loadSettings()` thất bại và rơi về giá trị mặc định, các `CHECK` bên dưới sẽ lộ ngay con số mặc định.
- Dòng 493: đặt các cờ auto theo tổ hợp riêng (`pumpAuto=false`, `lightAuto=false`, `rgbAuto=true`, `fanAuto=false`, `breatheAuto=true`) — đủ cả true lẫn false để kiểm tra cả hai chiều.
- Dòng 494: gọi `saveSettings()` thật: nó mở `preferences`, ghi 11 khoá (`putFloat` 5 ngưỡng, `putInt` cho `lightThreshold`, `putBool` 5 cờ) rồi đóng lại. Mock Preferences chuyển các lời gọi này thành cặp khoá–giá trị trong mảng `sPrefs`.
- Dòng 496–498: ghi đè mọi ngưỡng về 0 và đảo toàn bộ cờ so với lúc lưu. Bước này quan trọng: nếu bỏ qua, các `CHECK` có thể "may mắn" đúng nhờ giá trị còn sót trong biến, chứ không chứng minh được gì.
- Dòng 499: `loadSettings()` đọc lại từ "flash" giả. Lưu ý nó dùng giá trị mặc định cho khoá thiếu (`getFloat("soilLow", 400.0f)`…), nên nếu `saveSettings()` ghi hụt khoá nào thì `CHECK` tương ứng sẽ báo giá trị mặc định.
- Dòng 501–503: `CHECK_NEAR` với sai số 0.01 cho 3 ngưỡng đất/nước — chúng lưu dạng float nên cần so gần đúng; 111, 222, 133 phải quay về nguyên vẹn.
- Dòng 504: `lightThreshold` là `int` nên dùng `CHECK_EQ`, yêu cầu đúng 654 (mock lưu float 654.0 rồi `getInt` cắt về int).
- Dòng 505–506: hai ngưỡng nhiệt độ cũng là float, sai số 0.01, yêu cầu đúng 35.5 và 21.5.
- Dòng 507: kiểm tra đủ 5 cờ: `pumpAuto` và `lightAuto` phải false, `rgbAuto` true, `fanAuto` false, `breatheAuto` true — chứng minh kiểu bool được bảo toàn (mock lưu 0/1, `getBool` coi khác 0 là true).
- Dòng 508: đóng hàm.

#### T15 — testDemoFullRun() (chú thích dòng 510; thân hàm 492–542)

**Mục đích:** Chạy trọn một lượt trình diễn `startDemo()`/`updateDemo()` (các hoạt động 2–12) qua `simLoop()`, kiểm tra đủ các bước quan trọng diễn ra và sau khi demo kết thúc mọi cơ cấu phải được trả về trạng thái an toàn; chạy tiếp kịch bản hết nước để chắc chắn bước bơm bị bỏ qua.

```cpp
// T15: chạy demo đầy đủ (hoạt động 2-12)
static void testDemoFullRun() {
  printf("T15 demo: full sequence\n");
  resetState();
  primeSensors(900);
  CHECK(gWater > (int)waterThresholdLow);

  startDemo();
  CHECK(demoRunning());
  bool sawRelay = false, inBreatheStep = false;
  uint32_t breatheMin = 9999, breatheMax = 0;
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (relayState) sawRelay = true;
    if (strstr(demoStepName(), "Den tho")) {
      inBreatheStep = true;
      uint32_t p = currentLedPwm();
      if (p > breatheMax) breatheMax = p;
      if (p < breatheMin) breatheMin = p;
    }
  }
  CHECK(!demoRunning());
  CHECK(sawRelay);                       // HD7 bơm khi có nước
  CHECK(inBreatheStep);                  // bước HD3 đã chạy
  CHECK(breatheMax > 0);                 // và LED thực sự breathing
  CHECK(breatheMin < breatheMax);
  CHECK(!relayState);
  CHECK(!blueLedState);
  CHECK(!breatheLedState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);
  CHECK_EQ(mockPixel[0], 0);

  // Hết nước: bước bơm phải bị bỏ qua suốt lượt chạy
  resetState();
  primeSensors(100);
  CHECK(gWater < (int)waterThresholdLow);
  startDemo();
  sawRelay = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (relayState) sawRelay = true;
  }
  CHECK(!demoRunning());
  CHECK(!sawRelay);
  checkBuzzer(false);
}
```

- Dòng 511: khai báo hàm test.
- Dòng 512: in tên test.
- Dòng 513: `resetState()` — nền sạch, `breatheAuto = false` (quan trọng cho phần kiểm tra trạng thái cuối demo).
- Dòng 514: `primeSensors(900)` nạp DHT 26.0 °C / 60 %, ADC LDR = 1000, SOIL = 600, WATER = 900 (tự đảo theo `INVERT_WATER`), rồi đọc `SENSOR_FILTER_SIZE` lần cho bộ lọc trung bình đầy và đồng bộ `lastSensorRead`.
- Dòng 515: `CHECK(gWater > (int)waterThresholdLow)` — nước 900 > 200, điều kiện để bước bơm HD7 thực sự được phép chạy.
- Dòng 517: `startDemo()` — code thật dừng cối xay gió/cửa cho ăn, đặt `sRunning = true`, bước 0 (HD2) và thực thi hành động vào của bước.
- Dòng 518: `CHECK(demoRunning())` — demo đã chuyển sang trạng thái chạy.
- Dòng 519: hai cờ theo dõi: `sawRelay` (đã từng thấy bơm chạy chưa) và `inBreatheStep` (đã bước vào bước thở chưa).
- Dòng 520: `breatheMin` khởi tạo 9999, `breatheMax` khởi tạo 0 để ghi nhận khoảng PWM của hiệu ứng thở.
- Dòng 521: `cap = mockMillis() + 120000` — mốc chặn 120 000 ms mô phỏng; nếu demo bị "kẹt" không kết thúc, vòng lặp vẫn thoát và các `CHECK` sau sẽ báo lỗi thay vì treo test.
- Dòng 522–532: vòng lặp cho tới khi demo xong hoặc quá hạn:
  - Dòng 523: `mockAdvance(10)` — đẩy đồng hồ ảo thêm 10 ms (demo dùng `millis()` nên thời gian phải được đẩy mới có tiến triển).
  - Dòng 524: `simLoop()` — mô phỏng đúng phần điều khiển mỗi vòng của `loop()`: đọc chạm/PIR, đọc cảm biến mỗi `SENSOR_INTERVAL` (1000 ms), cắt bơm khi hết nước, và vì demo đang chạy nên gọi `updateDemo()` + `controlBreathe()`.
  - Dòng 525: nếu `relayState` bật ở bất kỳ thời điểm nào → đánh dấu `sawRelay` (chứng minh bước HD7 đã thực sự đóng relay).
  - Dòng 526–531: khi tên bước hiện tại chứa "Den tho" (bước HD3, tên không dấu "HD3: Den tho PWM"), bật cờ `inBreatheStep` và cập nhật `breatheMin`/`breatheMax` từ `currentLedPwm()`.
- Dòng 533: `CHECK(!demoRunning())` — demo đã tự kết thúc trong hạn 120 s, tức state machine của `updateDemo()` chạy hết 15 bước.
- Dòng 534: `CHECK(sawRelay)` — có nước nên bước HD7 đã bật bơm thật.
- Dòng 535: `CHECK(inBreatheStep)` — bước HD3 (đèn thở PWM) đã chạy.
- Dòng 536: `CHECK(breatheMax > 0)` — PWM thở từng lớn hơn 0, tức LED thực sự sáng.
- Dòng 537: `CHECK(breatheMin < breatheMax)` — PWM biến thiên (không phải hằng số), đúng bản chất hiệu ứng thở tăng/giảm dần.
- Dòng 538: `CHECK(!relayState)` — bơm đã tắt (bước HD7 có hành động ra `DA_PUMP_OFF`, và phần dọn cuối demo cũng `setRelay(false)`).
- Dòng 539: `CHECK(!blueLedState)` — LED xanh đã tắt trong phần dọn cuối demo.
- Dòng 540: `CHECK(!breatheLedState)` — chế độ thở cũng đã tắt; vì `breatheAuto = false` nên không được bật lại.
- Dòng 541: servo phải về đúng `WINDMILL_MIN_ANGLE` (0) — không để cánh gió/cửa cho ăn kẹt ở góc giữa.
- Dòng 542: `CHECK_EQ(currentLedPwm(), 0)` — PWM LED về 0, không còn dòng điện dư.
- Dòng 543: `checkBuzzer(false)` — còi tắt (bước HD8 bật còi đã được tắt khi kết thúc bước).
- Dòng 544: `CHECK_EQ(mockPixel[0], 0)` — dải LED RGB đã tắt về màu đen.
- Dòng 546: chú thích mở kịch bản thứ hai: hết nước.
- Dòng 547–549: `resetState()`, `primeSensors(100)` (nước cạn), `CHECK(gWater < (int)waterThresholdLow)` — xác nhận 100 < 200, tức điều kiện an toàn đã đúng.
- Dòng 550: `startDemo()` — chạy lại toàn bộ trình diễn.
- Dòng 551–552: đặt lại `sawRelay = false` và đặt hạn mới 120 000 ms.
- Dòng 553–557: vòng lặp tương tự, chỉ theo dõi `relayState`. Bên trong `demoAction()` (demo.cpp:59), hành động `DA_PUMP_ON` kiểm tra `gWater >= waterThresholdLow` trước khi `setRelay(true)`, ngược lại chỉ ghi cảnh báo — nên relay phải không bao giờ bật.
- Dòng 558: `CHECK(!demoRunning())` — demo vẫn kết thúc bình thường dù bước bơm bị bỏ qua.
- Dòng 559: `CHECK(!sawRelay)` — điểm cốt lõi: suốt lượt chạy demo hết nước, relay không hề bật một lần nào (không chạy bơm khô, bảo vệ bơm).
- Dòng 560: `checkBuzzer(false)` — sau demo còi phải im (bước HD8 bật rồi tự tắt khi hết bước).
- Dòng 561: đóng hàm.

#### D1 — testBoundaries() (chú thích dòng 563; thân hàm 545–590)

**Mục đích:** Kiểm tra từng ngưỡng điều khiển tại **đúng giá trị biên**: giá trị bằng ngưỡng phải xử sự khác giá trị lệch một đơn vị, đúng theo các phép so sánh nghiêm ngặt (`<`, `>`) trong code thật. Đây là các test bắt lỗi "off-by-one" — nếu code viết nhầm `<=`/`>=`, bơm/quạt/đèn sẽ đóng ngắt ngay tại ranh giới và gây nhấp nháy.

```cpp
// D1: biên ngưỡng chính xác
static void testBoundaries() {
  printf("D1  boundary: exact threshold comparisons\n");

  // Bơm: so sánh nghiêm ngặt < / >
  resetState();
  gWater = 900;
  gSoil = 400; controlPump(); CHECK(!relayState);            // == low: không khởi động
  gSoil = 399; controlPump(); CHECK(relayState);             // < low: khởi động
  gSoil = 700; controlPump(); CHECK(relayState);             // == high: vẫn bật
  gSoil = 701; controlPump(); CHECK(!relayState);            // > high: dừng
  gWater = 200; gSoil = 399; controlPump(); CHECK(relayState);  // == waterLow: được phép
  gWater = 199; controlPump(); CHECK(!relayState);           // < waterLow: bị chặn

  // Quạt: nghiêm ngặt > / <
  resetState();
  gTemp = 30;    controlFan(); CHECK(!fanState);             // == high: không khởi động
  gTemp = 30.01f; controlFan(); CHECK(fanState);             // > high: khởi động
  gTemp = 25;    controlFan(); CHECK(fanState);              // == low: vẫn bật
  gTemp = 24.99f; controlFan(); CHECK(!fanState);            // < low: dừng

  // Đèn: ngưỡng +/- 50 với so sánh nghiêm ngặt
  resetState();
  gLight = 450; controlLight(); CHECK(!blueLedState);        // == t-50: không bật
  gLight = 449; controlLight(); CHECK(blueLedState);         // < t-50: bật
  gLight = 550; controlLight(); CHECK(blueLedState);         // == t+50: vẫn bật
  gLight = 551; controlLight(); CHECK(!blueLedState);        // > t+50: tắt

  // Thời tiết: cùng dải +/- 50 và độ ẩm > 80
  resetState();
  gHum = 50;
  gLight = 550; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);
  gLight = 450; CHECK_EQ(weatherStatus(), WEATHER_NORMAL);
  gLight = 449; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 80;    CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);   // == 80 không tính là ẩm
  gHum = 81;    CHECK_EQ(weatherStatus(), WEATHER_RAIN);
  gLight = 551; CHECK_EQ(weatherStatus(), WEATHER_CLOUDY);
  gHum = 50.5f; CHECK_EQ(weatherStatus(), WEATHER_SUNNY);

  // Còi: nghiêm ngặt < waterLow
  resetState();
  gPir = false;
  gWater = 200; controlAlerts(); checkBuzzer(false);
  gWater = 199; controlAlerts(); checkBuzzer(true);
  gWater = 200; controlAlerts(); checkBuzzer(false);
}
```

- Dòng 564–565: khai báo hàm test và in tên test.
- Dòng 567: chú thích nhóm bơm — code thật (`controlPump()`, control.cpp:32/34) bật khi `gSoil < low` và tắt khi `gSoil > high`, đều là so sánh nghiêm ngặt.
- Dòng 568–569: `resetState()`; `gWater = 900` (đủ nước để không kích hoạt nhánh an toàn).
- Dòng 570: `gSoil = 400` **đúng bằng** `soilThresholdLow` → `CHECK(!relayState)`: chưa được bật vì cần nhỏ hơn thật sự.
- Dòng 571: `gSoil = 399` (lệch đúng 1) → bơm bật. Đây là chứng minh biên dưới là `<` chứ không phải `<=`.
- Dòng 572: `gSoil = 700` **đúng bằng** `soilThresholdHigh` → vẫn bật, vì chỉ tắt khi lớn hơn thật sự.
- Dòng 573: `gSoil = 701` → tắt. Cặp 700/701 chứng minh biên trên là `>`.
- Dòng 574: `gWater = 200` **đúng bằng** `waterThresholdLow` → vẫn được phép bật bơm; an toàn nước chỉ chặn khi nhỏ hơn thật sự (control.cpp:18).
- Dòng 575: `gWater = 199` → bơm bị tắt ngay lập tức (nhánh an toàn). Cặp 200/199 là biên sống còn: chỉ lệch 1 đơn vị ADC nhưng quyết định bơm có được chạy khô hay không.
- Dòng 577: chú thích nhóm quạt — `controlFan()` dùng `gTemp > high` để bật và `gTemp < low` để tắt.
- Dòng 578: `resetState()` đưa `tempThresholdHigh = 30`, `tempThresholdLow = 25`.
- Dòng 579: `gTemp = 30` đúng bằng ngưỡng cao → không bật.
- Dòng 580: `gTemp = 30.01f` (hơn đúng 0.01) → bật; chứng minh ngưỡng cao cũng nghiêm ngặt.
- Dòng 581: `gTemp = 25` đúng bằng ngưỡng thấp → quạt vẫn chạy (chưa tắt).
- Dòng 582: `gTemp = 24.99f` → tắt. Dải 25..30 là vùng trễ (hysteresis) chống quạt bật/tắt liên tục.
- Dòng 584–589: nhóm đèn xanh: `controlLight()` bật khi `gLight < lightThreshold − 50` và tắt khi `gLight > lightThreshold + 50` (mặc định 500 → hai mốc 450 và 550):
  - Dòng 586: `gLight = 450` đúng bằng mốc dưới → không bật.
  - Dòng 587: `449` → bật.
  - Dòng 588: `550` đúng bằng mốc trên → vẫn bật.
  - Dòng 589: `551` → tắt. Dải chết 450..550 rộng 100 đơn vị ADC để đèn không nhấp nháy khi ánh sáng dao động quanh ngưỡng.
- Dòng 591–601: nhóm thời tiết dùng lại đúng hai mốc ±50 và ngưỡng ẩm > 80:
  - Dòng 593: `gHum = 50` (khô).
  - Dòng 594: `gLight = 550` — không sáng (550 không > 550), không tối → `WEATHER_NORMAL` (0).
  - Dòng 595: `551` → sáng, khô → `WEATHER_SUNNY` (1).
  - Dòng 596: `450` — không tối (450 không < 450) → `WEATHER_NORMAL`.
  - Dòng 597: `449` → tối, khô → `WEATHER_CLOUDY` (2).
  - Dòng 598: `gHum = 80` **đúng bằng 80** → vẫn không tính là ẩm (điều kiện là `> 80.0`), trời tối → `WEATHER_CLOUDY`.
  - Dòng 599: `gHum = 81` → ẩm, tối → `WEATHER_RAIN` (3).
  - Dòng 600: `gLight = 551` (sáng) + ẩm → `WEATHER_CLOUDY` (2) — cùng độ ẩm nhưng sáng thì là "âm u", tối mới là "mưa".
  - Dòng 601: `gHum = 50.5f` (khô) + sáng → `WEATHER_SUNNY`. Cặp 80/81 và 50.5 kiểm tra cả ngưỡng ẩm lẫn việc độ ẩm thập phân không làm sai nhánh.
- Dòng 603: chú thích nhóm còi: cảnh báo dùng đúng điều kiện `gWater < waterThresholdLow` như nhánh an toàn của bơm.
- Dòng 604–605: `resetState()`; `gPir = false` để nhánh đèn PIR không can thiệp, chỉ kiểm tra còi.
- Dòng 606: `gWater = 200` đúng bằng ngưỡng → `checkBuzzer(false)`, còi im.
- Dòng 607: `gWater = 199` → `checkBuzzer(true)`, còi kêu; biên cảnh báo khớp chính xác biên an toàn của bơm.
- Dòng 608: `gWater = 200` trở lại → `checkBuzzer(false)`: còi phải tắt khi nước hồi phục, không được "kẹt kêu".
- Dòng 609: đóng hàm.

#### D2 — testIdempotency() (chú thích dòng 611; thân hàm 593–653)

**Mục đích:** Chứng minh mọi hàm setter của cơ cấu chấp hành có tính **idempotent**: gọi lại lệnh cũ với cùng trạng thái thì không ghi chân, không phát xung, không làm gì thêm. Đồng thời kiểm tra mức điện áp chân đúng với logic đã ghi.

```cpp
// D2: tính idempotent của actuator + nhất quán dây chân
static void testIdempotency() {
  printf("D2  actuators: idempotency + pin levels\n");
  resetState();
  setRelay(false);
  setFan(false);
  setBuzzer(false);
  setBlueLed(false);
  setBreatheLed(false);

  int d0 = mockDigitalWriteCount;
  setRelay(true);
  CHECK_EQ(mockDigitalWriteCount, d0 + 1);
  setRelay(true);                                // idempotent: không ghi lại
  CHECK_EQ(mockDigitalWriteCount, d0 + 1);
  checkRelayPin(true);
  setRelay(false);
  CHECK_EQ(mockDigitalWriteCount, d0 + 2);
  checkRelayPin(false);

  int d1 = mockDigitalWriteCount;
  setFan(true);
  CHECK_EQ(mockDigitalWriteCount, d1 + 1);
  setFan(true);
  CHECK_EQ(mockDigitalWriteCount, d1 + 1);
  CHECK_EQ(mockPinLevel[FAN_PIN], HIGH);
  setFan(false);
  CHECK_EQ(mockDigitalWriteCount, d1 + 2);
  CHECK_EQ(mockPinLevel[FAN_PIN], LOW);

#if BUZZER_PASSIVE
  int t0 = mockToneCalls, n0 = mockNoToneCalls;
  setBuzzer(true);
  setBuzzer(true);
  CHECK_EQ(mockToneCalls, t0 + 1);
  checkBuzzer(true);
  setBuzzer(false);
  CHECK_EQ(mockNoToneCalls, n0 + 1);
  checkBuzzer(false);
#else
  int d2 = mockDigitalWriteCount;
  setBuzzer(true);
  setBuzzer(true);
  CHECK_EQ(mockDigitalWriteCount, d2 + 1);
  checkBuzzer(true);
  setBuzzer(false);
  CHECK_EQ(mockDigitalWriteCount, d2 + 2);
  checkBuzzer(false);
#endif

  setBlueLed(true);
  int p0 = mockLedcWriteCount;
  setBlueLed(true);                              // idempotent: không ghi lại
  CHECK_EQ(mockLedcWriteCount, p0);
  CHECK_EQ(currentLedPwm(), 255);
  setBlueLed(false);
  CHECK_EQ(currentLedPwm(), 0);
  setBreatheLed(false);
  int p1 = mockLedcWriteCount;
  setBreatheLed(false);                          // đã tắt: không làm gì
  CHECK_EQ(mockLedcWriteCount, p1);
}
```

"Gọi lại lệnh cũ không tạo thêm tác động" nghĩa là: nếu trạng thái mục tiêu đã đúng (ví dụ bơm đang chạy mà lại ra lệnh `setRelay(true)`), setter thoát ngay ở dòng `if (relayState == on) return;` (actuators.cpp:10) — không ghi lại chân GPIO, không phát lại xung, không ghi thêm log. Điều này quan trọng vì `controlAlerts()`/`controlLight()`… được gọi mỗi vòng `loop()`, tức setter bị gọi liên tục với cùng một trạng thái; nếu không idempotent thì chân relay bị giật điện liên tục (nghe lạch cạch, giảm tuổi thọ relay), servo bị rung, buzzer bị phát lại xung, log tràn cổng serial và CPU bị đốt vô ích.

- Dòng 612–613: khai báo hàm test và in tên test.
- Dòng 614–619: `resetState()` (đã đặt mọi trạng thái về false) rồi chủ động gọi 5 setter về false. Vì trạng thái đã false sẵn, cả 5 lời gọi đều thoát sớm — cũng là minh hoạ idempotent, và quan trọng hơn là để chốt trạng thái chắc chắn trước khi đo.
- Dòng 621: `d0 = mockDigitalWriteCount` — chốt mốc đếm số lần `digitalWrite()` đã xảy ra.
- Dòng 622–623: `setRelay(true)` ghi chân một lần → bộ đếm tăng đúng 1.
- Dòng 624–625: gọi lại `setRelay(true)` khi relay đã bật → bộ đếm **không đổi** (`d0 + 1`). Đây là phép kiểm tra idempotent cốt lõi.
- Dòng 626: `checkRelayPin(true)` — xác nhận mức chân thật đúng (đã tính cực tính `RELAY_ACTIVE_LOW`: active-low thì bật = LOW).
- Dòng 627–629: tắt relay → bộ đếm `d0 + 2` (thêm đúng 1 lần ghi), mức chân đổi theo.
- Dòng 631–635: làm lại kịch bản với quạt: bật ghi 1 lần, gọi lặp không ghi thêm.
- Dòng 636: `CHECK_EQ(mockPinLevel[FAN_PIN], HIGH)` — quạt dùng logic thường, bật = mức HIGH.
- Dòng 637–639: tắt quạt → thêm 1 lần ghi, chân xuống LOW.
- Dòng 641–659: khối còi chia hai nhánh biên dịch tuỳ loại buzzer:
  - Nhánh `#if BUZZER_PASSIVE` (build `--full` với `-DBUZZER_PASSIVE=1`): đếm `mockToneCalls`/`mockNoToneCalls`; gọi `setBuzzer(true)` hai lần chỉ phát đúng 1 `tone()`, tắt phát đúng 1 `noTone()`.
  - Nhánh `#else` (build mặc định, buzzer chủ động): bật hai lần chỉ 1 `digitalWrite`, tắt thêm 1; `checkBuzzer()` xác nhận chân/mức đúng.
- Dòng 661–665: `setBlueLed(true)` lần đầu ghi PWM 255; chốt `p0 = mockLedcWriteCount` rồi gọi lại `setBlueLed(true)` khi đèn đã sáng → `mockLedcWriteCount` không đổi. `currentLedPwm() == 255` xác nhận độ sáng tối đa.
- Dòng 666–667: tắt đèn → PWM về 0 (đọc từ `mockPwm` qua `currentLedPwm()`).
- Dòng 668–671: `setBreatheLed(false)` khi chế độ thở đang tắt → thoát sớm, không ghi PWM; chốt `p1` trước và gọi lại lần nữa cũng không đổi bộ đếm. Chốt lại: cùng một trạng thái, gọi bao nhiêu lần cũng không sinh thêm tác động vật lý.
- Dòng 672: đóng hàm.

#### D3 — testTimingBoundaries() (chú thích dòng 674; thân hàm 656–706)

**Mục đích:** Kiểm tra chính xác các **mốc thời gian**: đúng `TOUCH_DEBOUNCE − 1` thì chưa xảy ra, đúng `TOUCH_DEBOUNCE` thì xảy ra; tương tự cho cửa cho ăn, hiệu ứng thở, cối xay gió; và ghi servo đúng tại hai góc biên 0°/180°.

```cpp
// D3: biên thời gian chính xác + ghi servo tại các mốc biên
static void testTimingBoundaries() {
  printf("D3  timing: exact interval boundaries\n");

  // Debounce chạm: >= TOUCH_DEBOUNCE
  resetState();
  mockAdvance(10000);
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  gTouch = true; controlTouch();                  // delta 0: bị nuốt
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(TOUCH_DEBOUNCE - 1);                // 299: bị nuốt
  gTouch = true; controlTouch();
  CHECK(!blueLedState);
  gTouch = false; controlTouch();
  mockAdvance(1);                                 // đúng 300: được phép
  gTouch = true; controlTouch();
  CHECK(blueLedState);

  // Cửa cho ăn: đóng đúng tại FEED_DURATION
  resetState();
  startFeed();
  mockAdvance(FEED_DURATION - 1); controlFeed(); CHECK(feedRunning());
  mockAdvance(1);                 controlFeed(); CHECK(!feedRunning());

  // Breathing: cập nhật đầu tiên đúng tại BREATHE_INTERVAL
  resetState();
  setBreatheLed(true);
  setLedPwm(0);
  lastBreatheUpdate = mockMillis();
  mockAdvance(BREATHE_INTERVAL - 1); controlBreathe(); CHECK_EQ(currentLedPwm(), 0);
  mockAdvance(1);                    controlBreathe(); CHECK_EQ(currentLedPwm(), 5);
  setBreatheLed(false);

  // Cối xay gió: bước quét chạy đúng tại WINDMILL_INTERVAL
  resetState();
  setWindmill(true);
  mockAdvance(1000);
  controlWindmill();                              // đồng bộ timer khoảng thời gian
  int c0 = mockServoWriteCount;
  mockAdvance(WINDMILL_INTERVAL - 1); controlWindmill(); CHECK_EQ(mockServoWriteCount, c0);
  mockAdvance(1);                     controlWindmill(); CHECK_EQ(mockServoWriteCount, c0 + 1);
  setWindmill(false);

  // Ghi servo tại đúng các mốc biên
  resetState();
  servoAngle = 0;   servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 0);
  servoAngle = 180; servo.write(servoAngle);   CHECK_EQ(mockServoAngle, 180);
  servoAngle = 0;   servo.write(servoAngle);
}
```

Vì sao phải dùng `mockAdvance` và kiểm tra đúng giá trị biên: firmware ra quyết định theo `millis()` (đồng hồ ảo trong test). `mockAdvance(n)` cộng thẳng `n` ms vào đồng hồ đó một cách tất định — không chờ thật, không phụ thuộc tốc độ máy, nên có thể kiểm chứng chính xác từng mốc ±1 ms. Kiểm tra đúng 29 ms "chưa đổi" và đúng 30 ms "mới đổi" bảo vệ chống lỗi off-by-one: nếu code viết `>` thay vì `>=`, thiết bị sẽ phản ứng chậm hơn một chu kỳ; còn nếu cập nhật mỗi vòng lặp không kiểm tra thời gian thì PWM/servo bị ghi dồn dập, gây nhiễu và tốn CPU.

- Dòng 675–676: khai báo hàm test và in tên test.
- Dòng 678: chú thích nhóm chạm — `controlTouch()` cho phép nhấn mới khi `now − lastTouch >= TOUCH_DEBOUNCE`.
- Dòng 679–680: `resetState()` (đồng hồ ảo về 0), rồi `mockAdvance(10000)` đưa đồng hồ lên mốc 10 000 ms để không dính các mốc 0.
- Dòng 681: `gTouch = false; controlTouch();` — nhả chạm trước để chốt `sPrevTouch = false` bên trong hàm. Hook `testResetControlStatics()` (được `resetState()` gọi) đã dọn `sPrevTouch`, nhưng test vẫn chốt tường minh trạng thái "không chạm" ngay tại đây — đảm bảo lần nhấn sau chắc chắn là sườn lên thật.
- Dòng 682: `lastTouch = mockMillis()` — đồng bộ mốc thời gian debounce bằng tay để phép đo delta bắt đầu từ đúng 10 000 ms.
- Dòng 683: `setBlueLed(false)` — tắt đèn chắc chắn (đã tắt nên không ghi).
- Dòng 684–685: nhấn ngay (`delta = 0`) → bị nuốt, đèn không bật. Biên dưới của debounce.
- Dòng 686: nhả chạm để lần nhấn kế tiếp lại là sườn lên.
- Dòng 687–689: đẩy đúng `TOUCH_DEBOUNCE − 1` = 299 ms rồi nhấn → vẫn bị nuốt. Đây là điểm "29 ms chưa đổi" của debounce chạm.
- Dòng 690: nhả chạm.
- Dòng 691–693: đẩy thêm đúng 1 ms → tổng 300 ms, vừa đủ `TOUCH_DEBOUNCE`; nhấn → được chấp nhận, đèn bật. Nếu code dùng `>` thay vì `>=`, test này sẽ thất bại ngay.
- Dòng 695: chú thích nhóm cửa cho ăn.
- Dòng 696–697: `resetState()` (đồng hồ về 0); `startFeed()` mở cửa tại mốc 0 và ghi góc mở.
- Dòng 698: sau 1999 ms (`FEED_DURATION − 1`), `controlFeed()` → vẫn đang mở.
- Dòng 699: thêm đúng 1 ms → tổng 2000 ms, cửa đóng đúng hạn. Sai một nhịp là thời lượng mở máng ăn lệch khỏi thiết kế.
- Dòng 701: chú thích nhóm thở PWM.
- Dòng 702–704: `resetState()`; `setBreatheLed(true)` bật chế độ thở (đặt độ sáng 0, hướng +1); `setLedPwm(0)` ghi PWM 0 để có mốc so sánh.
- Dòng 705: `lastBreatheUpdate = mockMillis()` — chủ động đồng bộ mốc cập nhật về thời điểm hiện tại (sau `mockAdvance(10000)`). `resetState()` đã đưa biến toàn cục này về 0, nhưng phép đo biên cần mốc bắt đầu đúng ngay trước `BREATHE_INTERVAL` nên test gán tay, không tin vào giá trị cũ.
- Dòng 706: sau `BREATHE_INTERVAL − 1` = 29 ms, `controlBreathe()` chưa cập nhật → PWM vẫn đúng 0.
- Dòng 707: thêm 1 ms → đúng 30 ms, cập nhật đầu tiên chạy: độ sáng 0 + 5 = 5 → PWM bằng 5. Đúng cả mốc thời gian lẫn bước tăng 5.
- Dòng 708: tắt chế độ thở để dọn trạng thái.
- Dòng 710: chú thích nhóm cối xay gió.
- Dòng 711–713: `resetState()`; `setWindmill(true)`; đẩy 1000 ms.
- Dòng 714: gọi `controlWindmill()` một lần để **đồng bộ mốc timer** về thời điểm hiện tại: `sWindmillUpdate` là static phạm vi file, hook reset đã đưa về 0 nhưng test muốn mốc bắt đầu đúng ngay trước `WINDMILL_INTERVAL`; lần ghi servo này không được tính vào phép đo (chốt `c0` ở dòng sau).
- Dòng 715: `c0 = mockServoWriteCount` — chốt số lần ghi servo.
- Dòng 716: sau đúng 29 ms (`WINDMILL_INTERVAL − 1`), bộ đếm ghi servo không đổi — chưa tới chu kỳ quét.
- Dòng 717: thêm đúng 1 ms → đủ 30 ms, ghi servo thêm đúng 1 lần (`c0 + 1`). Nhờ đếm số lần ghi, test chứng minh không có lần ghi thừa nào giữa hai mốc.
- Dòng 718: tắt cối xay gió.
- Dòng 720–724: kiểm tra ghi servo tại đúng hai mốc biên hành trình: ghi 0° → mock nhận đúng 0; ghi 180° → mock nhận đúng 180; rồi trả servo về 0°. Hai góc biên là giới hạn cơ khí, ghi lệch có thể làm kẹt bánh răng servo.
- Dòng 725: đóng hàm.

### 14.4. I1–I10 — integration test qua setup()/loop() thật (dòng 727–971)

Khác với các unit test ở mục trước (gọi thẳng từng hàm), nhóm test I1–I10 chạy **firmware thật**: gọi `setup()` và `loop()` của SmartFarm.ino, gọi thẳng các handler HTTP đã đăng ký. Ba "đồ nghề" dùng chung của nhóm này (đã định nghĩa ở dòng 162–181):

- `invoke(path, ...)` — "gõ URL" trong test: đặt tham số truy vấn giả bằng `mockServerSetArg()`, gọi handler của route bằng `mockServerInvoke()` (trả về `true` nếu route tồn tại; nếu không, `invoke` tự đánh dấu FAIL), rồi trả mã HTTP mà handler đã `send()`.
- `runLoop(ms)` — chạy vòng `loop()` thật trong thời gian mô phỏng: mỗi bước 10 ms gọi `mockAdvance()` (đẩy đồng hồ giả) rồi `loop()`.
- `has(chuỗi, mẫu)` — dò chuỗi con bằng `strstr` (xem dòng 70–72).

`setup()` chỉ được gọi **một lần duy nhất** ở I1; từ I2 trở đi các test tận dụng 14 route đã đăng ký sẵn đó. Bốn macro CHECK đã giải thích ở mục 14.1, không nhắc lại.

#### I1 — testInoBoot() (dòng 732–743)

**Mục đích:** Chạy `setup()` đúng một lần để xác nhận firmware khởi động đăng ký đủ 14 route HTTP và in banner/menu ra Serial.

```cpp
static void testInoBoot() {
  printf("I1  boot: setup() registers routes\n");
  mockSerialClear();
  mockServerReset();
  setup();
  CHECK_EQ(mockServerRouteCount(), 14);
  const char *routes[] = {"/", "/api", "/toggle", "/auto", "/rgb", "/servo", "/breathe",
                          "/on", "/off", "/windmill", "/feed", "/demo", "/threshold", "/favicon.ico"};
  for (const char *r : routes) CHECK(mockServerHasRoute(r));
  CHECK(has(mockSerialOutput(), "SMART FARM DEBUG MENU"));
  CHECK(has(mockSerialOutput(), "System ready"));
}
```

- Dòng 733: in mốc tên test để dễ đọc log khi chạy.
- Dòng 734: `mockSerialClear()` xóa cả output lẫn buffer nhập giả của Serial — nhờ vậy phần kiểm tra sau chỉ thấy đúng những gì `setup()` in ra.
- Dòng 735: `mockServerReset()` xóa sạch bảng route giả + tham số + response cuối; mục đích là buộc `setup()` phải tự đăng ký lại từ đầu, nhờ đó con số đếm được ở dòng 737 mới đáng tin.
- Dòng 736: gọi `setup()` thật của firmware (bật Serial, cấu hình GPIO/PWM, OLED, nạp cài đặt, WiFi, đăng ký route). Đây là lần gọi `setup()` duy nhất của nhóm I1–I10.
- Dòng 737: sau `setup()` phải có **đúng 14 route** — khớp đúng 14 lệnh `server.on(...)` trong SmartFarm.ino (dòng 158–172).
- Dòng 738–739: mảng 14 đường dẫn mong đợi: trang gốc `/`, API `/api`, các nút điều khiển, và `/favicon.ico`.
- Dòng 740: duyệt mảng, từng route một phải tồn tại trong mock (`mockServerHasRoute` trả về `false` thì CHECK báo FAIL).
- Dòng 741: Serial phải chứa chuỗi `"SMART FARM DEBUG MENU"` — do `printDebugMenu()` in ở cuối `setup()`.
- Dòng 742: Serial phải chứa `"System ready"` — dòng log cuối cùng của `setup()`.

#### I2 — testHttpApi() (dòng 746–768)

**Mục đích:** Chốt "hợp đồng" JSON của route `/api`: đúng kiểu nội dung, đủ trường, giá trị khớp với trạng thái giả đã biết.

```cpp
static void testHttpApi() {
  printf("I2  http: /api JSON\n");
  gTemp = 26.5f; gHum = 61.2f;
  gLight = 800; gSoil = 650; gWater = 900;
  gPir = true; gTouch = false;
  CHECK_EQ(invoke("/api"), 200);
  CHECK_STR(mockServerLastType(), "application/json");
  const char *b = mockServerLastBody();
  CHECK(has(b, "\"temp\":26.5"));
  CHECK(has(b, "\"hum\":61.2"));
  CHECK(has(b, "\"light\":800"));
  CHECK(has(b, "\"soil\":650"));
  CHECK(has(b, "\"water\":900"));
  CHECK(has(b, "\"weather\":1"));            // sáng + không khí khô -> nắng
  CHECK(has(b, "\"pir\":true"));
  CHECK(has(b, "\"touch\":false"));
  CHECK(has(b, "\"relay\":false"));
  CHECK(has(b, "\"windmill\":false"));
  CHECK(has(b, "\"feed\":false"));
  CHECK(has(b, "\"demo\":false"));
  CHECK(has(b, "\"pumpAuto\":"));
  CHECK(has(b, "\"breatheAuto\":"));
}
```

- Dòng 747: mốc tên test.
- Dòng 748–750: gán bộ giá trị cảm biến giả đã biết trước: 26.5 °C, 61.2 %, light 800, soil 650, water 900, PIR có người, touch không chạm.
- Dòng 751: `invoke("/api")` gọi handler thật `handleApi()`; CHECK_EQ yêu cầu mã HTTP trả về là **200** (thành công).
- Dòng 752: kiểu nội dung phải là `"application/json"` — nếu trả `text/html` thì trình duyệt/đối tác sẽ hiểu sai.
- Dòng 753: lấy body (chuỗi JSON) vào biến `b` để dò tiếp.
- Dòng 754–755: body phải chứa đúng `"temp":26.5` và `"hum":61.2` — in đúng 1 chữ số thập phân.
- Dòng 756–758: light/soil/water phải là 800/650/900 (số nguyên, không có phần thập phân).
- Dòng 759: `"weather":1` — trời sáng (light 800 > ngưỡng) và không khí khô (hum 61.2 thấp) nên `weatherStatus()` trả `WEATHER_SUNNY` = 1 (enum: NORMAL=0, SUNNY=1, CLOUDY=2, RAIN=3).
- Dòng 760–761: `"pir":true` (có người) và `"touch":false` (không chạm) — đúng như đã gán ở dòng 750.
- Dòng 762–765: các cờ ở trạng thái nghỉ phải là `false`: relay (bơm), windmill (cối xay), feed (cho ăn), demo (trình diễn).
- Dòng 766–767: chỉ cần **có mặt** hai khóa `"pumpAuto"` và `"breatheAuto"` (không kiểm giá trị) — đủ để chắc API không thiếu trường; phần kiểm tra giá trị chi tiết do test D5 (dùng parser JSON) lo.

#### I3 — testHttpLedOnOff() (dòng 771–780)

**Mục đích:** Kiểm tra hai route bật/tắt LED xanh dương (hoạt động 12): vừa đổi trạng thái phần cứng vừa trả đúng body xác nhận.

```cpp
static void testHttpLedOnOff() {
  printf("I3  http: /on /off\n");
  CHECK_EQ(invoke("/on"), 200);
  CHECK(blueLedState);
  CHECK(!lightAuto);
  CHECK_STR(mockServerLastBody(), "LED ON");
  CHECK_EQ(invoke("/off"), 200);
  CHECK(!blueLedState);
  CHECK_STR(mockServerLastBody(), "LED OFF");
}
```

- Dòng 772: mốc tên test.
- Dòng 773: gọi `/on` → phải trả HTTP 200.
- Dòng 774: biến trạng thái `blueLedState` phải bật `true`.
- Dòng 775: `lightAuto` phải bị tắt — quy tắc "bấm tay thì thoát chế độ tự động" của đèn.
- Dòng 776: body trả về phải **đúng nguyên văn** `"LED ON"` (CHECK_STR so cả chuỗi, không phải chuỗi con).
- Dòng 777: gọi `/off` → cũng phải 200.
- Dòng 778: `blueLedState` phải về `false`.
- Dòng 779: body phải đúng `"LED OFF"`.

#### I4 — testHttpMechanisms() (dòng 783–807)

**Mục đích:** Kiểm tra cơ cấu servo qua HTTP: cối xay gió bật/tắt, cho ăn mở cửa, và quy tắc "điều khiển tay thì giải phóng cơ cấu tự động" (HĐ6/HĐ10).

```cpp
static void testHttpMechanisms() {
  printf("I4  http: windmill / feed / servo arbitration\n");
  invoke("/windmill", "on", "true");
  CHECK(windmillOn);
  invoke("/windmill", "on", "false");
  CHECK(!windmillOn);

  invoke("/feed");
  CHECK(feedRunning());
  CHECK_EQ(mockServoAngle, FEED_OPEN_ANGLE);

  invoke("/servo", "angle", "45");           // điều khiển thủ công giải phóng cơ cấu
  CHECK_EQ(servoAngle, 45);
  CHECK_EQ(mockServoAngle, 45);
  CHECK(!feedRunning());
  invoke("/servo", "angle", "999");          // bị kẹp
  CHECK_EQ(servoAngle, 180);
  invoke("/servo", "angle", "-5");           // bị kẹp
  CHECK_EQ(servoAngle, 0);

  invoke("/demo");
  CHECK(demoRunning());
  runLoop(40000);                            // chạy toàn bộ demo qua loop()
  CHECK(!demoRunning());
}
```

- Dòng 784: mốc tên test.
- Dòng 785–786: gọi `/windmill?on=true` → cờ `windmillOn` phải bật.
- Dòng 787–788: gọi `/windmill?on=false` → cờ phải tắt (mỗi lần `invoke` đặt lại tham số trước khi gọi handler).
- Dòng 790–792: gọi `/feed` (không tham số) → `feedRunning()` phải `true` và góc servo giả phải bằng `FEED_OPEN_ANGLE` = **90°** (cửa máng ăn mở — xem config.h).
- Dòng 794–795: gọi `/servo?angle=45` → điều khiển thủ công, biến `servoAngle` = 45.
- Dòng 796: `mockServoAngle` cũng phải 45 — tức lệnh đã thực sự ghi ra servo phần cứng, không chỉ lưu biến.
- Dòng 797: `feedRunning()` phải `false` — điều khiển tay "giải phóng" cơ cấu cho ăn đang chạy (không tranh chấp servo).
- Dòng 798–799: góc 999 vượt biên bị **kẹp về 180** (giới hạn servo).
- Dòng 800–801: góc −5 bị kẹp về **0**.
- Dòng 803–804: gọi `/demo` → demo của bộ kit bắt đầu chạy.
- Dòng 805: `runLoop(40000)` cho `loop()` thật chạy 40 giây mô phỏng — tổng 15 bước demo khoảng 30,7 s nên demo kết thúc trọn vẹn bên trong khoảng này.
- Dòng 806: hết thời gian demo, cờ `demoRunning()` phải tự tắt.

#### I5 — testHttpControlRoutes() (dòng 810–860)

**Mục đích:** Gom các route điều khiển còn lại — `/toggle`, `/auto`, `/rgb`, `/threshold`: kiểm tra kẹp giá trị, tự sửa thứ tự ngưỡng, lưu bền vào Preferences và mã lỗi 400 khi tham số sai.

```cpp
static void testHttpControlRoutes() {
  printf("I5  http: toggle/auto/rgb/threshold\n");
  invoke("/toggle", "id", "pump");
  CHECK(!pumpAuto);
  CHECK(relayState);
  invoke("/toggle", "id", "pump");
  CHECK(!relayState);
  CHECK_EQ(invoke("/toggle", "id", "bogus"), 400);

  invoke("/auto", "id", "rgb", "on", "false");
  CHECK(!rgbAuto);

  invoke("/rgb", "r", "300", "g", "-5", "b", "128");
  CHECK_EQ(rgbR, 255);
  CHECK_EQ(rgbG, 0);
  CHECK_EQ(rgbB, 128);
  CHECK(!rgbAuto);
  CHECK_EQ(mockPixel[1], (255 << 16) | (0 << 8) | 128);

  invoke("/auto", "id", "breathe", "on", "true");
  CHECK(breatheAuto);
  CHECK(breatheLedState);
  runLoop(2000);
  uint32_t peak = 0;
  for (int i = 0; i < 60; i++) {
    mockAdvance(BREATHE_INTERVAL);
    loop();
    if (currentLedPwm() > peak) peak = currentLedPwm();
  }
  CHECK(peak > 0);                           // breathing được animate qua loop()
  invoke("/auto", "id", "breathe", "on", "false");
  CHECK(!breatheAuto);
  CHECK(!breatheLedState);
  CHECK_EQ(invoke("/auto", "id", "bogus", "on", "true"), 400);

  invoke("/threshold", "soilLow", "2000");
  CHECK_NEAR(soilThresholdLow, 2000, 0.01);
  CHECK(soilThresholdHigh > soilThresholdLow);   // tự sửa thứ tự
  CHECK_NEAR(mockPrefGet("soilLow", 0), 2000, 0.01);  // đã lưu bền
  invoke("/threshold", "soilLow", "9000");
  CHECK_NEAR(soilThresholdLow, 4095, 0.01);      // bị kẹp
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);
  CHECK_EQ(invoke("/threshold"), 400);           // không có tham số
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "tempLow", "50");         // dải nhiệt độ đảo được tự sửa
  CHECK_NEAR(tempThresholdLow, 50, 0.01);
  CHECK(tempThresholdHigh > tempThresholdLow);
  CHECK_NEAR(mockPrefGet("tempHigh", 0), tempThresholdHigh, 0.01);
  tempThresholdHigh = 30; tempThresholdLow = 25;
}
```

- Dòng 811: mốc tên test.
- Dòng 812–814: lần bấm `/toggle?id=pump` thứ nhất: `pumpAuto` phải tắt (thoát tự động) và `relayState` phải bật (bơm chạy bằng tay).
- Dòng 815–816: bấm lần hai: `relayState` phải tắt (bơm dừng tay); `pumpAuto` vẫn giữ `false` (test không yêu cầu bật lại).
- Dòng 817: `id=bogus` (thiết bị không tồn tại) → phải trả **400** (Bad Request).
- Dòng 819–820: `/auto?id=rgb&on=false` → tắt tự động RGB.
- Dòng 822: `/rgb?r=300&g=-5&b=128` — giá trị cố tình vượt biên để thử kẹp.
- Dòng 823–825: kênh R 300 bị kẹp về **255**, G −5 bị kẹp về **0**, B giữ **128**.
- Dòng 826: `rgbAuto` phải `false` — đặt màu bằng tay thì thoát chế độ tự động.
- Dòng 827: LED NeoPixel thứ 1 (chỉ số 1) phải mang mã màu `(255 << 16) | (0 << 8) | 128` = 0xFF0080 — kiểu đóng gói 0xRRGGBB; `setRgb` đặt cùng màu cho mọi LED nên chỉ cần mẫu 1 LED.
- Dòng 829–831: `/auto?id=breathe&on=true` → bật tự động thở: cờ `breatheAuto` và `breatheLedState` đều `true` (chế độ thở tiếp quản LED xanh).
- Dòng 832: chạy thêm 2 giây mô phỏng cho trạng thái ổn định.
- Dòng 833: biến `peak` lưu đỉnh PWM cao nhất quan sát được.
- Dòng 834–838: 60 lần lặp, mỗi lần đẩy đồng hồ đúng `BREATHE_INTERVAL` (30 ms) rồi gọi `loop()`; độ sáng PWM thay đổi dần nên phải ghi nhận được giá trị lớn hơn 0.
- Dòng 839: `CHECK(peak > 0)` — hiệu ứng thở **thực sự được animate** qua `loop()` (nếu logic đứng yên, PWM mãi là 0 và test đổ).
- Dòng 840–842: tắt tự động thở → cả `breatheAuto` lẫn `breatheLedState` phải về `false`.
- Dòng 843: `id=bogus` cho `/auto` → 400.
- Dòng 845–846: `/threshold?soilLow=2000` → ngưỡng đất dưới = 2000 (sai số cho phép 0.01).
- Dòng 847: ngưỡng trên phải **lớn hơn** ngưỡng dưới — firmware tự sửa thứ tự khi người dùng đặt ngược.
- Dòng 848: giá trị phải được lưu bền: `mockPrefGet("soilLow", 0)` trả 2000, chứng tỏ đã `put` vào Preferences.
- Dòng 849–851: đặt 9000 (vượt ADC 12 bit) → bị kẹp trần **4095**; ngưỡng trên cũng thành 4095 để không mâu thuẫn.
- Dòng 852: gọi `/threshold` **không tham số** → 400.
- Dòng 853: khôi phục cặp ngưỡng chuẩn (400/700) cho các test sau.
- Dòng 855–857: `/threshold?tempLow=50` — dải nhiệt độ bị đảo (low 50 > high cũ 30) → low = 50 và high được tự đẩy lên lớn hơn.
- Dòng 858: ngưỡng `tempHigh` mới cũng phải được lưu bền vào Preferences.
- Dòng 859: khôi phục dải nhiệt độ chuẩn 25–30.

#### I6 — testHttpRootFavicon() (dòng 863–871)

**Mục đích:** Kiểm tra trang chủ trả HTML đúng nội dung và favicon trả 204 (không có nội dung) để trình duyệt không tải ảnh.

```cpp
static void testHttpRootFavicon() {
  printf("I6  http: root page + favicon\n");
  CHECK_EQ(invoke("/"), 200);
  CHECK_STR(mockServerLastType(), "text/html");
  CHECK(has(mockServerLastBody(), "Smart Farm"));
  CHECK(has(mockServerLastBody(), "/windmill"));
  CHECK(has(mockServerLastBody(), "/demo"));
  CHECK_EQ(invoke("/favicon.ico"), 204);
}
```

- Dòng 864: mốc tên test.
- Dòng 865: gọi trang gốc `/` → HTTP 200.
- Dòng 866: kiểu nội dung phải là `"text/html"` để trình duyệt hiển thị trang.
- Dòng 867: body phải chứa tiêu đề `"Smart Farm"`.
- Dòng 868–869: trang phải có liên kết điều khiển `/windmill` (cối xay) và `/demo` (trình diễn) — đảm bảo giao diện không sót nút.
- Dòng 870: `/favicon.ico` phải trả **204 No Content** thay vì 404 — cách "im lặng" chuẩn mực với favicon.

#### I7 — testOled() (dòng 874–903)

**Mục đích:** Kiểm tra khung trạng thái vẽ lên OLED (giả) ở các trạng thái khác nhau: đủ cảm biến, IP, WiFi mất, cối xay chạy, đang cho ăn.

```cpp
static void testOled() {
  printf("I7  oled: status frame\n");
  mockWifiStatus = WL_CONNECTED;
  gTemp = 26.5f; gHum = 61; gLight = 800; gSoil = 650; gWater = 900;
  relayState = false;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  const char *t = mockOledText();
  CHECK(has(t, "Smart Farm"));
  CHECK(has(t, "TT: Nang"));
  CHECK(has(t, "T:26.5C H:61%"));
  CHECK(has(t, "Light: 800"));
  CHECK(has(t, "Soil: 650"));
  CHECK(has(t, "H2O:900 P:OFF"));
  CHECK(has(t, "IP:192.168.1.50"));
  CHECK(has(t, "San sang"));

  mockWifiStatus = WL_DISCONNECTED;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "WiFi: ---"));
  mockWifiStatus = WL_CONNECTED;

  windmillOn = true;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "Coi xay: ON"));
  windmillOn = false;
  startFeed();
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "Cho an..."));
  stopFeed();
}
```

- Dòng 875: mốc tên test.
- Dòng 876: giả lập WiFi đang kết nối (`WL_CONNECTED`) để OLED hiện IP.
- Dòng 877–878: đặt bộ cảm biến cố định và tắt bơm (`relayState = false`).
- Dòng 879: gọi thẳng `updateOLED(...)` — hàm vẽ khung trạng thái thật, nhưng mọi lệnh vẽ đi vào màn hình giả.
- Dòng 880: `mockOledText()` trả về toàn bộ text đã in lên OLED giả (gom theo từng dòng qua `setCursor`).
- Dòng 881: có tiêu đề `"Smart Farm"`.
- Dòng 882: `"TT: Nang"` — dòng thời tiết: light 800 là sáng, hum 61 khô → "Nang" (nắng).
- Dòng 883: `"T:26.5C H:61%"` — nhiệt độ 1 số thập phân, độ ẩm số nguyên.
- Dòng 884–885: giá trị ánh sáng 800 và độ ẩm đất 650 hiện đúng.
- Dòng 886: `"H2O:900 P:OFF"` — mức nước 900, bơm (P = Pump) đang OFF.
- Dòng 887: `"IP:192.168.1.50"` — IP giả do mock WiFi cung cấp.
- Dòng 888: `"San sang"` — trạng thái nghỉ ("Sẵn sàng", viết không dấu cho font OLED).
- Dòng 890–891: chuyển WiFi sang mất kết nối rồi vẽ lại.
- Dòng 892: màn hình phải hiện `"WiFi: ---"` thay cho IP.
- Dòng 893: trả WiFi về kết nối cho các phần sau.
- Dòng 895–896: bật cối xay rồi vẽ lại.
- Dòng 897: phải có dòng `"Coi xay: ON"`.
- Dòng 898–900: tắt cối xay, cho ăn rồi vẽ lại.
- Dòng 901: phải có dòng `"Cho an..."` — OLED phản ánh đúng trạng thái máng ăn.
- Dòng 902: `stopFeed()` dọn trạng thái cho các test sau.

#### I8 — testSerialMenu() (dòng 906–937)

**Mục đích:** Kiểm tra menu debug qua Serial: 8 phím `1`–`8` mở đúng mục, menu được in lại, phím `0` thoát, phím lạ báo "Invalid".

```cpp
static void testSerialMenu() {
  printf("I8  serial: debug menu commands\n");
  mockSerialClear();

  struct { char cmd; const char *marker; } cases[] = {
    {'1', "--- SENSORS ---"},
    {'2', "--- ACTUATORS ---"},
    {'3', "--- CONTROL ---"},
    {'4', "--- WEBSERVER ---"},
    {'5', "--- WIFI ---"},
    {'6', "--- OLED ---"},
    {'7', "--- SETTINGS ---"},
    {'8', "--- LOGGER ---"},
  };
  for (auto &c : cases) {
    mockSerialClear();
    mockSerialPush(&c.cmd);
    loop();
    CHECK(has(mockSerialOutput(), c.marker));
    CHECK(has(mockSerialOutput(), "Chon: "));       // in lại menu
    mockSerialPush("0");
    loop();
    CHECK(has(mockSerialOutput(), "Running normally"));
  }

  mockSerialClear();
  mockSerialPush("x");                               // không hợp lệ
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));
  CHECK(has(mockSerialOutput(), "Chon: "));
  mockSerialClear();
}
```

- Dòng 907: mốc tên test.
- Dòng 908: xóa Serial trước khi bắt đầu.
- Dòng 910–919: bảng đối chiếu 8 phím với dấu hiệu (marker) mà mỗi mục debug in ra, ví dụ phím `1` → `"--- SENSORS ---"`.
- Dòng 920: duyệt từng ca kiểm thử.
- Dòng 921: xóa output trước mỗi ca để chỉ đọc kết quả của ca này.
- Dòng 922: `mockSerialPush(&c.cmd)` đẩy ký tự phím vào buffer nhập giả của Serial.
- Dòng 923: gọi `loop()` — vòng lặp thật đọc phím, bật `sDebugMode` và chạy `handleDebugCmd()`.
- Dòng 924: output phải chứa marker của đúng mục (mở đúng chức năng).
- Dòng 925: cuối mỗi mục, menu phải in lại (`"Chon: "`) để người dùng chọn tiếp.
- Dòng 926–927: đẩy phím `0` rồi chạy `loop()` để thoát chế độ debug.
- Dòng 928: phải thấy dòng `"Running normally"` (thực tế firmware in `"Running normally..."` — test chỉ dò chuỗi con).
- Dòng 931–935: ca phím lạ `'x'`: `loop()` phải in `"Invalid"` và in lại menu `"Chon: "`.
- Dòng 936: dọn Serial lần cuối.

#### I9 — testSerialDemo() (dòng 940–956)

**Mục đích:** Kiểm tra phím `9` khởi động demo qua Serial, tên bước demo hiện trên OLED, và demo kết thúc êm qua `loop()`.

```cpp
static void testSerialDemo() {
  printf("I9  serial: '9' demo + OLED step name\n");
  mockSerialClear();
  mockSerialPush("9");
  loop();
  CHECK(demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO START"));

  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "HD2: Thap sang"));      // tên bước trên OLED

  runLoop(40000);
  CHECK(!demoRunning());
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK(has(mockOledText(), "San sang"));
}
```

- Dòng 941: mốc tên test.
- Dòng 942–944: xóa Serial, đẩy phím `'9'` rồi gọi `loop()` — firmware phải gọi `startDemo()`.
- Dòng 945: `demoRunning()` phải `true` — demo đã bật.
- Dòng 946: Serial phải có thông báo `"DEMO START"`.
- Dòng 948: vẽ OLED ngay khi demo vừa chạy.
- Dòng 949: màn hình phải hiện tên **bước đang chạy** `"HD2: Thap sang"` (bước đầu tiên của kịch bản, dòng demo.cpp:23).
- Dòng 951: `runLoop(40000)` chạy hết ~30,7 s kịch bản demo qua `loop()` thật.
- Dòng 952: kết thúc, `demoRunning()` phải `false`.
- Dòng 953: Serial phải có `"DEMO DONE"`.
- Dòng 954–955: vẽ lại OLED — giao diện phải quay về trạng thái nghỉ `"San sang"`.

#### I10 — testWifiRecovery() (dòng 959–970)

**Mục đích:** Kiểm tra phần quản lý WiFi trong `loop()`: khi mất mạng đủ lâu thì log "WiFi lost" và đến khi nối lại thì log "WiFi reconnected".

```cpp
static void testWifiRecovery() {
  printf("I10 wifi: loss + reconnect logging\n");
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi lost"));
  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
}
```

- Dòng 960: mốc tên test.
- Dòng 961: giả lập WiFi đang mất kết nối (`WL_DISCONNECTED`).
- Dòng 962: `lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1` — lùi mốc kiểm tra quá chu kỳ 10 giây đúng 1 ms, đảm bảo lần `loop()` tới chắc chắn đã đến hạn thử lại (nếu để nguyên, `manageWiFi()` sẽ chờ đủ 10 s mới làm gì).
- Dòng 963: xóa Serial để chỉ đọc log của vòng lặp này.
- Dòng 964: gọi `loop()` thật → `manageWiFi()` thấy mất mạng + đến hạn → ghi log lỗi 401.
- Dòng 965: Serial phải chứa `"WiFi lost"` (chuỗi đầy đủ là `"WiFi lost, reconnecting..."`).
- Dòng 966: giả lập WiFi nối lại được.
- Dòng 967–968: xóa Serial rồi gọi `loop()` thêm một vòng.
- Dòng 969: Serial phải có `"WiFi reconnected"` — firmware phát hiện vừa nối lại và thông báo đúng **một lần** (nhờ cờ `sWarned` trong `manageWiFi()`; việc chỉ log một lần được kiểm tra kỹ hơn ở test D8 — `testWifiThrottle()`).

### 14.5. Bộ đọc JSON tự viết — parseFlatJson và các hàm phụ (dòng 972–1060)

Để kiểm tra "hợp đồng" JSON của `/api`, bộ test **tự viết một parser JSON tối giản** thay vì dùng thư viện. Vì sao?

- **Không phụ thuộc thư viện:** test được biên dịch và chạy trên máy tính (host) với các file mock, không có sẵn ArduinoJson. Tự viết giữ bộ test độc lập, biên dịch nhanh, không lo lệch phiên bản.
- **Kiểm tra nghiêm ngặt đúng 21 khóa:** parser kèm bảng `API_KEYS` (21 khóa) buộc response `/api` phải đúng số lượng, đúng tên, đúng kiểu (số/bool/chuỗi), không thừa không thiếu không trùng. Chỉ cần đổi tên một trường mà quên cập nhật test là đổ ngay.
- **Chỉ cần JSON phẳng:** `/api` trả object một tầng, không lồng nhau, nên parser "flat" là đủ và dễ đọc hơn cả thư viện.

```cpp
// Parser flat-JSON tối giản dùng để kiểm tra contract của /api.
struct JsonPair { std::string key, raw, type; };   // kiểu: s(tring) n(umber) b(ool) z(null)
```

- Dòng 976: chú thích nêu mục đích của parser (kiểm tra contract của `/api`).
- Dòng 977: `JsonPair` gồm 3 chuỗi: `key` (tên khóa), `raw` (giá trị **nguyên văn** đúng như trong JSON) và `type` (kiểu: `"s"` string, `"n"` number, `"b"` bool, `"z"` null). Giữ `raw` nguyên văn giúp test so khớp chính xác cả chuỗi lẫn số.

```cpp
static bool parseFlatJson(const char *text, std::vector<JsonPair> &out, std::string &err) {
  const char *p = text;
  auto skipWs = [&]() { while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++; };
  skipWs();
  if (*p != '{') { err = "not a JSON object"; return false; }
  p++; skipWs();
  if (*p == '}') {
    p++; skipWs();
    if (*p != '\0') { err = "trailing garbage"; return false; }
    return true;
  }
  while (true) {
    skipWs();
    if (*p != '"') { err = "key is not a string"; return false; }
    p++;
    std::string key;
    while (*p && *p != '"') key += *p++;
    if (*p != '"') { err = "unterminated key"; return false; }
    p++; skipWs();
    if (*p != ':') { err = "missing ':'"; return false; }
    p++; skipWs();

    JsonPair pair;
    pair.key = key;
    if (*p == '"') {
      p++;
      pair.type = "s";
      while (*p && *p != '"') {
        if (*p == '\\') {
          p++;
          if (!*p) { err = "bad escape"; return false; }
        }
        pair.raw += *p++;
      }
      if (*p != '"') { err = "unterminated string"; return false; }
      p++;
    } else if (strncmp(p, "true", 4) == 0) {
      pair.type = "b"; pair.raw = "true"; p += 4;
    } else if (strncmp(p, "false", 5) == 0) {
      pair.type = "b"; pair.raw = "false"; p += 5;
    } else if (strncmp(p, "null", 4) == 0) {
      pair.type = "z"; pair.raw = "null"; p += 4;
    } else {
      char *end = nullptr;
      double v = strtod(p, &end);
      if (end == p) { err = "invalid value"; return false; }
      (void)v;
      pair.type = "n";
      pair.raw.assign(p, end - p);
      p = end;
    }
    out.push_back(pair);
    skipWs();
    if (*p == ',') { p++; continue; }
    if (*p == '}') { p++; break; }
    err = "expected ',' or '}'";
    return false;
  }
  skipWs();
  if (*p != '\0') { err = "trailing garbage"; return false; }
  return true;
}
```

Cách hoạt động từng bước:

- Dòng 979: chữ ký hàm: nhận chuỗi `text`, đổ kết quả vào `out`, ghi thông báo lỗi vào `err`; trả `true` nếu JSON hợp lệ.
- Dòng 980: con trỏ `p` là "ngón tay" duyệt lần lượt từng ký tự của chuỗi.
- Dòng 981: lambda `skipWs` nhảy qua mọi ký tự trắng (space, tab, `\n`, `\r`) — nhờ nó, parser chấp nhận JSON in trên một dòng lẫn nhiều dòng.
- Dòng 982–983: bỏ khoảng trắng đầu, rồi ký tự đầu tiên phải là `{` (mở object); sai thì báo `"not a JSON object"` và dừng.
- Dòng 984: đã qua `{`, bỏ tiếp khoảng trắng.
- Dòng 985–989: nhánh đặc biệt object rỗng `{}`: sau `}` chỉ được phép hết chuỗi; còn ký tự lạ thì báo `"trailing garbage"` (chống rác sau JSON).
- Dòng 990: vòng lặp vô hạn đọc từng cặp khóa–giá trị; thoát bằng `break` khi gặp `}`, hoặc `return false` khi gặp lỗi.
- Dòng 991–992: bỏ khoảng trắng, bắt buộc ký tự tiếp theo là `"` mở khóa; nếu không (khóa không phải chuỗi) → lỗi `"key is not a string"`.
- Dòng 993–995: gom từng ký tự của khóa vào biến `key` cho đến khi gặp `"` đóng; khóa không hỗ trợ escape (đủ dùng vì tên trường là chữ ASCII cố định).
- Dòng 996–997: thiếu nháy đóng → `"unterminated key"`; có thì nhảy qua `"` rồi bỏ khoảng trắng.
- Dòng 998–999: bắt buộc có dấu `:` ngăn cách khóa và giá trị (`"missing ':'"`), rồi bỏ khoảng trắng trước giá trị.
- Dòng 1001–1002: tạo `JsonPair` mới và gán `pair.key` vừa đọc.
- Dòng 1003–1005: nếu gặp `"` → đây là giá trị **chuỗi**: đánh dấu type `"s"` và nhảy qua nháy mở.
- Dòng 1006–1011: đọc từng ký tự vào `pair.raw`; nếu gặp dấu `\` (escape) thì nhảy qua nó, bắt buộc còn ký tự phía sau (`"bad escape"` nếu chuỗi cụt giữa escape), rồi lấy ký tự bị escape đó. Nói cách khác parser chấp nhận escape nhưng lưu ký tự trần.
- Dòng 1013–1014: phải gặp nháy đóng, nếu hết chuỗi trước thì `"unterminated string"`; sau đó nhảy qua nháy đóng.
- Dòng 1015–1016: nhận dạng `true` bằng `strncmp` 4 ký tự → type `"b"`, raw `"true"`, tiến con trỏ 4 bước.
- Dòng 1017–1018: nhận dạng `false` (5 ký tự) → type `"b"`, raw `"false"`.
- Dòng 1019–1020: nhận dạng `null` (4 ký tự) → type `"z"`, raw `"null"`.
- Dòng 1021–1028: nhánh còn lại coi là **số**: gọi `strtod` (hàm chuẩn của C) để đọc số từ vị trí `p`; nếu `end == p` tức không đọc được ký tự số nào → lỗi `"invalid value"`. `(void)v` chỉ để tắt cảnh báo biến không dùng (giá trị số không cần, vì chỉ giữ raw). `assign(p, end - p)` lưu đúng đoạn text số nguyên văn, type `"n"`, rồi nhảy `p` tới cuối số.
- Dòng 1030: cặp khóa–giá trị đã đọc xong → thêm vào vector `out`.
- Dòng 1031–1033: sau giá trị, bỏ khoảng trắng: gặp `,` thì sang cặp kế tiếp (`continue`); gặp `}` thì kết thúc object (`break`).
- Dòng 1034–1035: ký tự nào khác cũng là lỗi `"expected ',' or '}'"`.
- Dòng 1037–1039: sau `}` cuối cùng, bỏ khoảng trắng rồi **bắt buộc hết chuỗi**; còn ký tự lạ → `"trailing garbage"`; sạch sẽ thì trả `true`.

Tóm lại, parser xử lý 4 dạng giá trị (chuỗi, true/false, null, số), bỏ khoảng trắng ở mọi vị trí hợp lý, và coi **mọi sai lệch** là lỗi kèm mô tả tiếng Anh ngắn trong `err` (khóa không phải chuỗi, thiếu `:`, chuỗi hở, escape cụt, rác cuối chuỗi...).

```cpp
struct ApiKey { const char *key; char type; };
static const ApiKey API_KEYS[] = {
  {"temp", 'n'}, {"hum", 'n'}, {"light", 'n'}, {"soil", 'n'}, {"water", 'n'}, {"weather", 'n'},
  {"pir", 'b'}, {"touch", 'b'}, {"relay", 'b'}, {"fan", 'b'}, {"led", 'b'}, {"breathe", 'b'},
  {"windmill", 'b'}, {"feed", 'b'}, {"demo", 'b'}, {"demoStep", 's'},
  {"pumpAuto", 'b'}, {"lightAuto", 'b'}, {"rgbAuto", 'b'}, {"fanAuto", 'b'}, {"breatheAuto", 'b'}
};
static const int API_KEY_COUNT = (int)(sizeof(API_KEYS) / sizeof(API_KEYS[0]));
```

- Dòng 1042: `ApiKey` là cặp (tên khóa, ký tự kiểu mong đợi) — kiểu viết tắt đúng như trong `JsonPair.type`.
- Dòng 1043–1048: "bảng vàng" **21 khóa** bắt buộc của `/api`: 6 số (`temp`, `hum`, `light`, `soil`, `water`, `weather`), 14 bool (`pir`, `touch`, `relay`, `fan`, `led`, `breathe`, `windmill`, `feed`, `demo`, `pumpAuto`, `lightAuto`, `rgbAuto`, `fanAuto`, `breatheAuto`) và 1 chuỗi (`demoStep`). Đủ 21 = 6 + 14 + 1.
- Dòng 1049: `API_KEY_COUNT` tự tính bằng `sizeof` — thêm/bớt khóa trong bảng thì con số này tự đúng, không phải sửa tay.

```cpp
static const JsonPair *findPair(const std::vector<JsonPair> &pairs, const char *key) {
  for (const auto &p : pairs) if (p.key == key) return &p;
  return nullptr;
}
```

- Dòng 1051–1053: `findPair` dò tuyến tính trong danh sách cặp đã parse, trả con trỏ tới cặp có khóa trùng, hoặc `nullptr` nếu không có.
- Dòng 1054: kết thúc hàm — không tìm thấy thì trả `nullptr`.

```cpp
static double numOf(const std::vector<JsonPair> &pairs, const char *key) {
  const JsonPair *p = findPair(pairs, key);
  return p ? strtod(p->raw.c_str(), nullptr) : -1e9;
}
```

- Dòng 1056–1058: `numOf` lấy giá trị số của một khóa: tìm cặp (dòng 1057), có thì ép chuỗi `raw` thành `double` bằng `strtod`; **không có thì trả −1e9** — một số cực nhỏ, không thể trùng giá trị cảm biến thật, nên mọi so sánh sẽ lộ lỗi "thiếu khóa" ngay.
- Dòng 1059: đóng hàm.

Các hàm này (cùng parser) được dùng ở mục sau — `validateApiResponse()` và test D5 `testApiJsonContract()` (dòng 1061 trở đi, trong đó có `boolOf()` bắt đầu tại dòng 1061) — để khẳng định `/api` trả **đúng 21 khóa, đúng kiểu, không trùng lặp**, và giá trị số/bool khớp với trạng thái thật của firmware.

### 14.6. validateApiResponse + D5 — hợp đồng JSON 21 khóa (dòng 1061–1173)

Phần này kiểm tra /api có trả về đúng "contract" (hợp đồng dữ liệu) đã thỏa thuận giữa firmware và client hay không: đúng 21 khóa, đúng kiểu dữ liệu từng khóa, và giá trị phải khớp với các biến trạng thái thật. Bảng kỳ vọng nằm ngay trên vùng này: mảng `API_KEYS` (dòng 1043–1048) liệt kê 21 khóa với kiểu `'n'` number, `'b'` bool, `'s'` string; `API_KEY_COUNT` (dòng 1049) đếm ra 21.

#### boolOf() (dòng 1061–1064)

```cpp
static bool boolOf(const std::vector<JsonPair> &pairs, const char *key) {
  const JsonPair *p = findPair(pairs, key);
  return p && p->raw == "true";
}
```

- Dòng 1061: hàm phụ nhận danh sách cặp khóa–giá trị đã parse (`pairs`) và tên khóa cần tra; trả về `bool`.
- Dòng 1062: `findPair` (dòng 1051) duyệt tuần tự, trả con trỏ tới cặp có khóa trùng hoặc `nullptr` nếu không có.
- Dòng 1063: nếu không tìm thấy (`p` null) thì trả `false`; nếu thấy thì so chuỗi thô `raw` với `"true"` — chỉ đúng khi JSON ghi đúng `true`. Với kiểu bool, `parseFlatJson` (dòng 1015–1018) lưu raw là `"true"`/`"false"` nên so sánh chuỗi là đủ; mọi giá trị khác như `false`, `1`, `"true"` (chuỗi) đều bị coi là false.

#### validateApiResponse() (dòng 1067–1116)

```cpp
// Kiểm tra contract đầy đủ cho response /api cuối cùng
static bool validateApiResponse(std::string &err) {
  if (mockServerLastCode() != 200) { err = "HTTP code != 200"; return false; }
  if (strcmp(mockServerLastType(), "application/json") != 0) { err = "wrong content type"; return false; }

  std::vector<JsonPair> pairs;
  if (!parseFlatJson(mockServerLastBody(), pairs, err)) return false;

  if ((int)pairs.size() != API_KEY_COUNT) {
    char b[64]; snprintf(b, sizeof(b), "key count %d != %d", (int)pairs.size(), API_KEY_COUNT);
    err = b;
    return false;
  }
  std::set<std::string> seen;
  for (const auto &p : pairs) {
    if (!seen.insert(p.key).second) { err = "duplicate key: " + p.key; return false; }
  }
  for (int i = 0; i < API_KEY_COUNT; i++) {
    const JsonPair *p = findPair(pairs, API_KEYS[i].key);
    if (!p) { err = std::string("missing key: ") + API_KEYS[i].key; return false; }
    if (p->type[0] != API_KEYS[i].type) { err = std::string("bad type: ") + API_KEYS[i].key; return false; }
  }

  if (fabs(numOf(pairs, "temp") - gTemp) > 0.06) { err = "temp mismatch"; return false; }
  if (fabs(numOf(pairs, "hum") - gHum) > 0.06) { err = "hum mismatch"; return false; }
  if ((long)numOf(pairs, "light") != gLight) { err = "light mismatch"; return false; }
  if ((long)numOf(pairs, "soil") != gSoil) { err = "soil mismatch"; return false; }
  if ((long)numOf(pairs, "water") != gWater) { err = "water mismatch"; return false; }
  if ((int)numOf(pairs, "weather") != (int)weatherStatus()) { err = "weather mismatch"; return false; }

  if (boolOf(pairs, "pir") != gPir) { err = "pir mismatch"; return false; }
  if (boolOf(pairs, "touch") != gTouch) { err = "touch mismatch"; return false; }
  if (boolOf(pairs, "relay") != relayState) { err = "relay mismatch"; return false; }
  if (boolOf(pairs, "fan") != fanState) { err = "fan mismatch"; return false; }
  if (boolOf(pairs, "led") != blueLedState) { err = "led mismatch"; return false; }
  if (boolOf(pairs, "breathe") != breatheLedState) { err = "breathe mismatch"; return false; }
  if (boolOf(pairs, "windmill") != windmillOn) { err = "windmill mismatch"; return false; }
  if (boolOf(pairs, "feed") != feedRunning()) { err = "feed mismatch"; return false; }
  if (boolOf(pairs, "demo") != demoRunning()) { err = "demo mismatch"; return false; }
  if (boolOf(pairs, "pumpAuto") != pumpAuto) { err = "pumpAuto mismatch"; return false; }
  if (boolOf(pairs, "lightAuto") != lightAuto) { err = "lightAuto mismatch"; return false; }
  if (boolOf(pairs, "rgbAuto") != rgbAuto) { err = "rgbAuto mismatch"; return false; }
  if (boolOf(pairs, "fanAuto") != fanAuto) { err = "fanAuto mismatch"; return false; }
  if (boolOf(pairs, "breatheAuto") != breatheAuto) { err = "breatheAuto mismatch"; return false; }

  const JsonPair *ds = findPair(pairs, "demoStep");
  const char *expectedStep = demoRunning() ? demoStepName() : "";
  if (ds->raw != expectedStep) { err = "demoStep mismatch"; return false; }

  return true;
}
```

- Dòng 1066: comment gốc — "Kiểm tra contract đầy đủ cho response /api cuối cùng".
- Dòng 1067: nhận tham chiếu `err` để ghi lý do thất bại; hàm trả `true` nếu hợp đồng đúng.
- Dòng 1068: mã HTTP phải là 200 (thành công), nếu khác ghi lỗi và dừng ngay.
- Dòng 1069: header `Content-Type` phải đúng `application/json`; client không được nhận kiểu dữ liệu khác.
- Dòng 1071–1072: parse thân phản hồi bằng parser flat-JSON tự viết; parser thất bại thì trả false (parser đã điền `err`).
- Dòng 1074–1077: số cặp khóa phải đúng bằng 21; nếu sai thì in cả số thực tế lẫn số mong đợi để dễ debug.
- Dòng 1079–1082: dùng `std::set` (tập hợp) để phát hiện khóa trùng — `insert(...).second` là `false` nghĩa là khóa đã tồn tại. Nhờ vậy 21 khóa phải là 21 khóa phân biệt, không được lặp.
- Dòng 1083–1087: với từng khóa trong bảng `API_KEYS`: tìm khóa, thiếu thì lỗi "missing key"; kiểu (`p->type[0]`) phải khớp ký tự trong bảng (`'n'` number, `'b'` bool, `'s'` string, `'z'` null — quy ước ở dòng 977), sai thì lỗi "bad type".
- Dòng 1089–1090: temp và hum là số thực, cho phép lệch 0.06 — vì giá trị in ra JSON bị làm tròn khi format nên không thể so tuyệt đối.
- Dòng 1091–1093: light, soil, water là số nguyên; ép `(long)` trước khi so để tránh sai số kiểu float.
- Dòng 1094: weather phải khớp `(int)weatherStatus()` — trạng thái thời tiết hiện tại của firmware.
- Dòng 1096–1101: các cờ cảm biến và thiết bị: pir, touch (cảm biến), relay, fan, led, breathe (thiết bị) phải khớp biến trạng thái tương ứng.
- Dòng 1102–1104: windmill, feed, demo phải khớp trạng thái thật (`windmillOn`, `feedRunning()`, `demoRunning()`).
- Dòng 1105–1109: 5 cờ chế độ tự động: pumpAuto, lightAuto, rgbAuto, fanAuto, breatheAuto.
- Dòng 1111–1113: demoStep là string; nếu demo đang chạy thì phải bằng `demoStepName()`, ngược lại phải là chuỗi rỗng `""` (đúng quy ước của firmware). `ds` chắc chắn không null vì khóa demoStep đã được kiểm tra tồn tại ở vòng lặp dòng 1083.
- Dòng 1115: mọi điều kiện đều qua — hợp đồng JSON hợp lệ.

#### D5 — testApiJsonContract() (dòng 1119–1173; thân hàm 1100–1152)

**Mục đích:** Xác minh /api luôn trả JSON đúng hợp đồng 21 khóa — với trạng thái biết trước, trạng thái demo/cho ăn, và 200 trạng thái ngẫu nhiên.

```cpp
// D5: JSON contract nghiêm ngặt của /api
static void testApiJsonContract() {
  printf("D5  api: strict JSON contract (21 keys)\n");
  std::string err;

  // Trạng thái đã biết
  gTemp = 26.5f; gHum = 61.2f;
  gLight = 800; gSoil = 650; gWater = 900;
  gPir = true; gTouch = false;
  invoke("/api");
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());

  // Trạng thái demo: cờ demo + chuỗi step phải cập nhật trực tiếp
  invoke("/demo");
  CHECK(demoRunning());
  invoke("/api");
  err.clear();
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());
  CHECK(has(mockServerLastBody(), "\"demo\":true"));
  CHECK(has(mockServerLastBody(), demoStepName()));
  runLoop(40000);
  CHECK(!demoRunning());

  // Trạng thái cho ăn
  startFeed();
  invoke("/api");
  err.clear();
  if (!validateApiResponse(err)) printf("  FAIL: %s\n", err.c_str());
  CHECK(err.empty());
  CHECK(has(mockServerLastBody(), "\"feed\":true"));
  stopFeed();

  // 200 trạng thái ngẫu nhiên: JSON phải luôn hợp lệ và nhất quán
  std::mt19937 rng(1234);
  int bad = 0;
  for (int i = 0; i < 200; i++) {
    gTemp = (float)(rng() % 500) / 10.0f;
    gHum = (float)(rng() % 101);
    gLight = (int)(rng() % 4096);
    gSoil = (int)(rng() % 4096);
    gWater = (int)(rng() % 4096);
    gPir = (rng() % 2) != 0;
    gTouch = (rng() % 2) != 0;
    invoke("/api");
    err.clear();
    if (!validateApiResponse(err)) {
      bad++;
      if (bad == 1) printf("  FAIL: %s\n", err.c_str());
    }
  }
  CHECK_EQ(bad, 0);
}
```

- Dòng 1120: in tên test — "strict JSON contract (21 keys)" nghĩa là hợp đồng JSON nghiêm ngặt với 21 khóa.
- Dòng 1123–1126: đặt trạng thái đã biết: nhiệt độ 26.5°C, độ ẩm 61.2%, ánh sáng 800, đất 650, nước 900, có người (pir) và không chạm (touch) — các biến `g...` mô phỏng cảm biến toàn cục.
- Dòng 1127–1129: gọi /api rồi kiểm tra hợp đồng; nếu sai in lý do, và `CHECK(err.empty())` xác nhận không có lỗi (biến `err` do `validateApiResponse` điền).
- Dòng 1131–1137: bật demo rồi gọi /api ngay; JSON phải hợp lệ trong lúc demo đang chạy.
- Dòng 1138: `has` (dòng 70) là tìm chuỗi con bằng `strstr`; kiểm tra thân phản hồi chứa đúng đoạn `"demo":true`.
- Dòng 1139: thân phản hồi phải chứa tên bước demo hiện tại (`demoStepName()`) — tức demoStep cập nhật trực tiếp theo thời gian thực.
- Dòng 1140–1141: chạy mô phỏng 40 giây (bước 10ms) cho demo kết thúc, rồi xác nhận demo đã dừng.
- Dòng 1143–1150: trạng thái cho ăn: `startFeed()` rồi /api phải chứa `"feed":true`; sau đó `stopFeed()` dọn trạng thái.
- Dòng 1153: `std::mt19937 rng(1234)` — bộ sinh số giả ngẫu nhiên với seed (hạt giống) cố định là 1234. Vì thuật toán sinh số là tất định theo seed, dãy số sinh ra giống hệt nhau ở mọi lần chạy → test lặp lại được (reproducible). Nếu test fail, lập trình viên chạy lại vẫn gặp đúng trạng thái gây lỗi để sửa; nếu seed lấy theo đồng hồ thì mỗi lần một dãy khác, rất khó tái hiện lỗi.
- Dòng 1156: temp = `(rng() % 500) / 10.0` → số thực trong khoảng 0.0–49.9°C.
- Dòng 1157: hum = 0–100%.
- Dòng 1158–1160: light, soil, water = 0–4095, đúng dải ADC 12 bit của ESP32.
- Dòng 1161–1162: pir và touch ngẫu nhiên true/false.
- Dòng 1163–1168: gọi /api và kiểm tra hợp đồng với từng trạng thái; đếm số lần sai vào `bad`, chỉ in lỗi đầu tiên để log không bị ngập 200 dòng.
- Dòng 1170: `CHECK_EQ(bad, 0)` — không được phép có bất kỳ trạng thái nào trả JSON sai hợp đồng.
- Dòng 1172–1173: dòng trống và comment `// D6: ...` — mở đầu mục 14.7 bên dưới.

### 14.7. D6 — thân phản hồi chính xác + ngưỡng biên (dòng 1174–1269)

Mục này kiểm tra hai loại "hợp đồng" khác: (1) thân phản hồi HTTP của các route phải đúng từng ký tự, và (2) các ngưỡng cấu hình phải xử lý đúng tại biên — giá trị lớn nhất/nhỏ nhất, giá trị âm, chuỗi không phải số.

#### D6 — testResponseExactness() (dòng 1174–1269; thân hàm 1155–1248)

**Mục đích:** Xác minh body của các route (/on, /off, /toggle, /auto, /rgb, /breathe, /windmill, /feed, /demo, /threshold, /servo) đúng chính xác từng ký tự, và các ngưỡng (soil, water, temp, light) được kẹp/sửa đúng tại biên.

```cpp
// D6: độ chính xác body HTTP + biên ngưỡng
static void testResponseExactness() {
  printf("D6  http: exact bodies + threshold edges\n");

  CHECK_EQ(invoke("/on"), 200);
  CHECK_STR(mockServerLastBody(), "LED ON");
  CHECK_STR(mockServerLastType(), "text/plain");
  CHECK_EQ(invoke("/off"), 200);
  CHECK_STR(mockServerLastBody(), "LED OFF");

  CHECK_EQ(invoke("/toggle", "id", "pump"), 200);
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK_EQ(invoke("/toggle", "id", "nope"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");
  CHECK_EQ(invoke("/auto", "id", "nope", "on", "true"), 400);
  CHECK_STR(mockServerLastBody(), "unknown id");
  CHECK_EQ(invoke("/auto", "id", "light", "on", "TRUE"), 200);
  CHECK(!lightAuto);                       // chỉ "true" mới bật auto

  invoke("/rgb", "r", "0", "g", "255", "b", "255");
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbG, 255); CHECK_EQ(rgbB, 255);
  CHECK_STR(mockServerLastBody(), "ok");

  invoke("/breathe");
  CHECK_STR(mockServerLastBody(), "ok");
  CHECK(breatheLedState);
  CHECK(!breatheAuto);
  invoke("/breathe");
  CHECK(!breatheLedState);

  invoke("/windmill");                     // không tham số -> tắt
  CHECK_STR(mockServerLastBody(), "windmill off");
  invoke("/windmill", "on", "true");
  CHECK_STR(mockServerLastBody(), "windmill on");
  CHECK(windmillOn);
  invoke("/windmill", "on", "false");
  CHECK_STR(mockServerLastBody(), "windmill off");

  invoke("/feed");
  CHECK_STR(mockServerLastBody(), "feeding");
  CHECK(feedRunning());
  runLoop(2100);
  CHECK(!feedRunning());

  invoke("/demo");
  CHECK_STR(mockServerLastBody(), "demo started");
  CHECK(demoRunning());
  runLoop(32000);
  CHECK(!demoRunning());

  CHECK_EQ(invoke("/threshold"), 400);
  CHECK_STR(mockServerLastBody(), "no params");
  invoke("/threshold", "soilLow", "4095");
  CHECK_NEAR(soilThresholdLow, 4095, 0.01);
  CHECK_NEAR(soilThresholdHigh, 4095, 0.01);        // mức sửa tốt nhất tại biên
  // Ngưỡng bằng nhau không được làm bơm bật/tắt liên tục (guard điều khiển):
  gWater = 900; gSoil = 100; relayState = false; pumpAuto = true;
  controlPump();
  CHECK(!relayState);
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "soilLow", "-5");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);
  CHECK_NEAR(soilThresholdHigh, 700, 0.01);         // không đổi (vẫn > low)
  invoke("/threshold", "soilLow", "abc");
  CHECK_NEAR(soilThresholdLow, 0, 0.01);            // toFloat("abc") trả về 0
  soilThresholdLow = 400; soilThresholdHigh = 700;

  invoke("/threshold", "waterLow", "99999");
  CHECK_NEAR(waterThresholdLow, 4095, 0.01);
  waterThresholdLow = 200;

  invoke("/threshold", "tempLow", "60");
  CHECK_NEAR(tempThresholdLow, 60, 0.01);
  CHECK_NEAR(tempThresholdHigh, 61, 0.01);          // giữ nghiêm ngặt trên low
  invoke("/threshold", "tempLow", "25");
  CHECK_NEAR(tempThresholdLow, 25, 0.01);
  invoke("/threshold", "tempHigh", "0");
  CHECK_NEAR(tempThresholdHigh, 26, 0.01);          // sửa thành low + 1
  tempThresholdHigh = 30; tempThresholdLow = 25;
  invoke("/threshold", "light", "5000");
  CHECK_EQ(lightThreshold, 4095);
  lightThreshold = 500;

  invoke("/rgb");                                    // không tham số -> toàn số 0
  CHECK_EQ(rgbR, 0); CHECK_EQ(rgbG, 0); CHECK_EQ(rgbB, 0);
  CHECK(!rgbAuto);
  invoke("/servo", "angle", "0");
  CHECK_EQ(servoAngle, 0);
  invoke("/servo", "angle", "180");
  CHECK_EQ(servoAngle, 180);
  invoke("/servo", "angle", "abc");
  CHECK_EQ(servoAngle, 0);
  servoAngle = 0;
}
```

- Dòng 1175: in tên test — "exact bodies + threshold edges" nghĩa là thân phản hồi chính xác + các giá trị ngay tại biên (ngưỡng).
- Dòng 1177–1179: /on phải trả mã 200, body đúng chính xác `LED ON`, kiểu `text/plain`. Vì `CHECK_STR` dùng `strcmp` (so từng ký tự, phân biệt hoa/thường và khoảng trắng), chỉ cần thừa một dấu cách hay ký tự xuống dòng là fail — điều này giữ giao diện ổn định cho mọi client đang đọc chuỗi này.
- Dòng 1180–1181: /off tương tự với body `LED OFF`.
- Dòng 1183–1186: /toggle với `id=pump` trả 200 và body `ok`; `id=nope` trả 400 (yêu cầu sai) cùng thông báo `unknown id`.
- Dòng 1187–1188: /auto với id lạ cũng trả 400 và `unknown id`.
- Dòng 1189–1190: /auto `light` với `on=TRUE` (chữ hoa) trả 200 nhưng cờ `lightAuto` phải vẫn tắt — firmware chỉ nhận đúng chuỗi `"true"`, qua đó kiểm tra việc phân biệt chữ hoa/thường.
- Dòng 1192–1194: /rgb đặt từng kênh màu; ba biến `rgbR`, `rgbG`, `rgbB` phải lần lượt là 0, 255, 255 và body là `ok`.
- Dòng 1196–1201: /breathe lần đầu bật hiệu ứng (`breatheLedState` true) và phải tắt chế độ auto (`breatheAuto` false); lần thứ hai thì tắt hiệu ứng.
- Dòng 1203–1209: /windmill không tham số mặc định tắt, body `windmill off`; `on=true` bật, body `windmill on`; `on=false` tắt lại.
- Dòng 1211–1215: /feed trả body `feeding`, trạng thái cho ăn bật; chạy mô phỏng 2100ms (hơn thời lượng cho ăn ~2 giây một chút) thì phải tự kết thúc — đây là kiểm tra tại biên thời gian.
- Dòng 1217–1221: /demo trả `demo started`, demo chạy; sau 32 giây mô phỏng thì demo phải kết thúc.
- Dòng 1223–1224: /threshold không có tham số trả 400 kèm body `no params` (không có tham số).
- Dòng 1225–1227: `soilLow=4095` — 4095 là giá trị ADC lớn nhất (12 bit, mức ẩm cao nhất). Firmware kẹp giá trị vào dải hợp lệ và sửa để high không nhỏ hơn low, nên cả `soilThresholdLow` lẫn `soilThresholdHigh` đều thành 4095 (chú thích gốc: "mức sửa tốt nhất tại biên").
- Dòng 1228–1231: kiểm tra guard (bộ bảo vệ) chống bơm bật/tắt liên tục khi hai ngưỡng bằng nhau (vùng trễ bằng 0): đặt đất rất khô (100), nước đủ (900), bơm đang tắt, pumpAuto bật, gọi `controlPump()` — kỳ vọng relay vẫn tắt (không tự bật).
- Dòng 1232: khôi phục ngưỡng đất về 400/700 trước các kiểm tra tiếp theo.
- Dòng 1234–1236: `soilLow=-5` bị kẹp về 0 (không cho ngưỡng âm); `soilThresholdHigh` giữ nguyên 700 vì vẫn lớn hơn low.
- Dòng 1237–1238: `soilLow=abc` — chuỗi không phải số nên `toFloat("abc")` trả về 0, ngưỡng thành 0 (comment gốc trong code).
- Dòng 1239: khôi phục ngưỡng đất.
- Dòng 1241–1243: `waterLow=99999` vượt trần ADC nên bị kẹp về 4095; sau đó khôi phục về 200.
- Dòng 1245–1247: `tempLow=60` được nhận đúng; `tempThresholdHigh` tự động thành 61 để luôn cao hơn low ít nhất 1 đơn vị (giữ nghiêm ngặt high > low).
- Dòng 1248–1249: `tempLow=25` được nhận đúng 25.
- Dòng 1250–1251: `tempHigh=0` nhỏ hơn low (25) nên bị sửa thành low + 1 = 26.
- Dòng 1252: khôi phục temp về 25/30.
- Dòng 1253–1255: `light=5000` vượt trần 4095 nên bị kẹp về 4095; khôi phục về 500.
- Dòng 1257–1259: /rgb không tham số đặt cả ba kênh về 0 và tắt rgbAuto.
- Dòng 1260–1261: /servo với `angle=0` (biên dưới) hợp lệ → `servoAngle` = 0.
- Dòng 1262–1263: /servo với `angle=180` (biên trên) hợp lệ → `servoAngle` = 180.
- Dòng 1264–1265: góc `abc` không phải số → về 0 (giá trị an toàn).
- Dòng 1266: khôi phục `servoAngle` về 0 sau test.
- Dòng 1268–1269: dòng trống và comment `// D7: ...` — mở đầu mục test OLED tiếp theo (ngoài phạm vi 14.7).


### 14.8. D7 + D8 — layout OLED từng dòng và biên serial (dòng 1270–1391)

#### D7 — testOledLines() (dòng 1270–1297)

**Mục đích:** Gọi hàm thật `updateOLED()` rồi soi từng dòng mà nó vẽ ra: đúng nội dung, đúng định dạng, và mọi dòng đều ≤ 21 ký tự kể cả khi số liệu ở giá trị xấu nhất.

```cpp
static void testOledLines() {
  printf("D7  oled: per-row layout + 21-char line limit\n");
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "255.255.255.255";
  gTemp = 26.5f; gHum = 61; gLight = 800; gSoil = 650; gWater = 900;
  relayState = false; windmillOn = false;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK_STR(mockOledLine(0), "Smart Farm");
  CHECK_STR(mockOledLine(1), "TT: Nang");
  CHECK_STR(mockOledLine(2), "T:26.5C H:61%");
  CHECK_STR(mockOledLine(3), "Light: 800");
  CHECK_STR(mockOledLine(4), "Soil: 650");
  CHECK_STR(mockOledLine(5), "H2O:900 P:OFF");
  CHECK_STR(mockOledLine(6), "IP:255.255.255.255");
  CHECK_STR(mockOledLine(7), "San sang");

  // Giá trị xấu nhất: mỗi dòng vẫn phải vừa 21 ký tự (128 px / 6 px)
  gTemp = 99.9f; gHum = 100; gLight = 4095; gSoil = 4095; gWater = 4095;
  relayState = true;
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  for (int r = 0; r < mockOledLineCount(); r++) {
    CHECK(strlen(mockOledLine(r)) <= 21);
    CHECK(strlen(mockOledLine(r)) > 0);
  }
  CHECK_STR(mockOledLine(5), "H2O:4095 P:ON");
  relayState = false;
  mockWifiIp = "192.168.1.50";
}
```

- Dòng 1270–1271: khai báo hàm test; câu `printf` chỉ để in tên test khi chạy.
- Dòng 1272: nạp mock `mockWifiStatus = WL_CONNECTED` — “WiFi đang kết nối”, cần cho dòng IP ở dòng 6.
- Dòng 1273: nạp IP dài nhất có thể `"255.255.255.255"` (15 ký tự) — đây là ca xấu nhất cho dòng “IP:…”.
- Dòng 1274: đặt thẳng 5 giá trị cảm biến (không cần đọc ADC) để các dòng giữa có số đẹp, dễ đối chiếu.
- Dòng 1275: tắt bơm và cối xay để thấy rõ nhánh `P:OFF` và nhánh “San sang”.
- Dòng 1276: gọi hàm thật `updateOLED()` (src/oled.cpp:18). Mock OLED ghi lại từng dòng theo lời gọi `setCursor(0, y)`: y chia 8 là số thứ tự dòng (mock.cpp:302–306).
- Dòng 1277: dòng 0 phải là tiêu đề “Smart Farm”.
- Dòng 1278: dòng 1 là “TT: ” + mô tả thời tiết; gLight 800 > ngưỡng 500+50 và gHum 61 ≤ 80 nên `weatherStatus()` trả WEATHER_SUNNY → chữ “Nang” (sensors.cpp:93–99, oled.cpp:9–16).
- Dòng 1279: dòng 2 = “T:” + nhiệt độ 1 số thập phân (26.5) + “C H:” + độ ẩm 0 số thập phân (61) + “%”.
- Dòng 1280–1281: dòng 3, 4 = “Light: 800”, “Soil: 650” (chú ý có dấu cách sau dấu hai chấm).
- Dòng 1282: dòng 5 = “H2O:900 P:OFF” — phần `P:` theo cờ `relayState` (đang tắt).
- Dòng 1283: dòng 6 = “IP:” + địa chỉ IP mà mock trả về (tổng 18 ký tự).
- Dòng 1284: dòng 7 = “San sang” vì demo không chạy, cối xay tắt, máng ăn đang đóng (oled.cpp:66–76).
- Dòng 1286–1288: chuyển sang bộ giá trị xấu nhất: 99.9 °C, độ ẩm 100 %, các cảm biến ADC chạm trần 4095 (ADC 12 bit: 2^12 − 1); bật bơm để ép nhánh `P:ON`.
- Dòng 1289: vẽ lại toàn bộ màn hình với dữ liệu mới.
- Dòng 1290–1292: quét cả 8 dòng (`mockOledLineCount()` luôn trả 8); `CHECK(<= 21)` vì màn rộng 128 px, font mặc định mỗi ký tự 6 px → 128/6 ≈ 21,3 nên tối đa 21 ký tự; `CHECK(> 0)` chắc rằng không dòng nào bị bỏ trống.
- Dòng 1294: riêng dòng 5 phải đúng “H2O:4095 P:ON” — số 4 chữ số và trạng thái ON vẫn không làm dòng vượt 21 ký tự.
- Dòng 1295–1296: dọn dẹp: trả bơm về tắt, trả IP mặc định cho các test sau.

#### D8 — testSerialEdges() (dòng 1300–1381)

**Mục đích:** Kiểm tra các ca biên khi gõ lệnh qua serial (dòng trống, chỉ CR, byte rác, dòng rác dài) và hành vi chế độ debug: tạm dừng tự động hóa, từ chối phím lạ, rồi khôi phục quy tắc an toàn ngay khi thoát.

```cpp
static void testSerialEdges() {
  printf("D8  serial: edge cases + debug mode\n");

  mockSerialClear();
  mockSerialPush("\r\n");                    // dòng trống: bỏ qua
  loop();
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  mockSerialClear();
  mockSerialPush("\r");                      // chỉ CR: bỏ qua
  loop();
  CHECK_EQ(strlen(mockSerialOutput()), 0);

  char badByte[2] = {(char)0xFF, 0};         // byte rác: bị từ chối
  mockSerialClear();
  mockSerialPush(badByte);
  loop();
  CHECK(has(mockSerialOutput(), "Invalid"));

  mockSerialClear();
  for (int i = 0; i < 64; i++) mockSerialPush("x");   // dòng rác dài
  for (int i = 0; i < 64; i++) loop();
  CHECK_EQ(mockSerialPending(), 0);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "Invalid"), 64);

  // '0' ở chế độ thường được xử lý an toàn (in thông báo thoát, vẫn ở chế độ thường)
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), ">> Exit debug."));

  // Chế độ debug tạm dừng automation (chế độ bảo trì)...
  resetState();
  gWater = 900; gSoil = 100; pumpAuto = true;
  controlPump();
  CHECK(relayState);
  mockSerialClear();
  mockSerialPush("3");
  loop();
  CHECK(has(mockSerialOutput(), "--- CONTROL ---"));
  gWater = 50;
  mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
  lastSensorRead = mockMillis();
  loop();
  CHECK(relayState);                          // chế độ debug: automation tạm dừng
  // trong chế độ debug mọi phím khác đều bị từ chối
  mockSerialClear();
  mockSerialPush("x");
  loop();
  CHECK(has(mockSerialOutput(), "Enter '0' to exit debug."));
  // ...và loop đầu tiên sau khi thoát khôi phục quy tắc an toàn
  mockSerialClear();
  mockSerialPush("0");
  loop();
  CHECK(has(mockSerialOutput(), "Running normally..."));
  CHECK(!relayState);

  // Menu debug '5' khi WiFi mất sẽ đi qua nhánh kết nối lại
  resetState();
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  mockSerialPush("5");
  loop();
  CHECK(has(mockSerialOutput(), "--- WIFI ---"));
  CHECK_EQ(mockWifiReconnectCalls, 1);
  mockSerialPush("0");
  loop();
  mockWifiStatus = WL_CONNECTED;

  // Mức DEBUG bật dòng cảm biến định kỳ và tag DEBUG
  resetState();
  gMinLogLevel = LOG_DEBUG;
  mockSerialClear();
  logSensors();
  CHECK(has(mockSerialOutput(), "[SENSORS]"));
  debugLogger();
  CHECK(has(mockSerialOutput(), "[DEBUG]"));
  gMinLogLevel = LOG_INFO;

  resetState();
}
```

- Dòng 1300–1301: khai báo hàm test; câu `printf` chỉ để in tên test.
- Cách “bơm” byte rác: test nạp ký tự vào bộ đệm nhập serial giả lập bằng `mockSerialPush()` (mock.cpp:144) rồi gọi `loop()` thật. Mỗi vòng `loop()` chỉ đọc đúng 1 byte qua `Serial.available()/Serial.read()` (SmartFarm.ino:170–171), nên muốn ăn hết N byte phải gọi loop N lần.
- Dòng 1303–1307: bơm chuỗi `"\r\n"` (dòng trống) rồi loop 2 lần; cả '\r' lẫn '\n' đều rơi vào nhánh “bỏ qua” (SmartFarm.ino:172–173) nên `mockSerialOutput()` phải rỗng.
- Dòng 1309–1312: chỉ một ký tự '\r' cũng bị bỏ qua, không in gì.
- Dòng 1314–1318: byte rác 0xFF: `Serial.read()` trả 255, gán vào biến `char` thành −1 — không phải '9', không nằm trong '0'..'8' nên rơi vào nhánh cuối và in “>> Invalid. Enter 0-9.” (SmartFarm.ino:180–183). CHECK chỉ cần thấy chữ “Invalid”.
- Dòng 1320–1324: gửi 64 ký tự 'x' liền nhau; chạy đúng 64 vòng loop, mỗi vòng “ăn” 1 ký tự. `mockSerialPending() == 0` chứng minh không sót byte nào trong bộ đệm; `countOccurrences("Invalid") == 64` chứng minh từng ký tự rác đều bị từ chối riêng, không bị nuốt cụm.
- Dòng 1327–1330: gõ '0' lúc đang ở chế độ thường: code tạm bật cờ debug rồi `handleDebugCmd('0')` lập tức tắt lại và in “>> Exit debug.” — thoát an toàn, máy vẫn ở chế độ thường.
- Dòng 1333–1336: dựng tình huống bơm đang chạy: nước 900, đất 100 (rất khô), bật auto rồi gọi `controlPump()` thật → bơm ON, CHECK xác nhận.
- Dòng 1338–1340: gõ '3' để vào menu debug Control; `debugControl()` in “--- CONTROL ---” (logger.cpp:106–112).
- Dòng 1341–1345: nước tụt còn 50, cập nhật luôn `mockAdc[WATER_PIN]` cho khớp (đảo cực nếu INVERT_WATER), dời `lastSensorRead` để chu kỳ đọc 1 giây không đọc đè. Gọi `loop()`: vì đang ở chế độ debug, `loop()` thoát sớm tại `if (sDebugMode) return;` (SmartFarm.ino:194) TRƯỚC dòng an toàn — nên bơm vẫn chạy. Đây là chủ ý “chế độ bảo trì tạm dừng tự động hóa”, và CHECK khẳng định đúng điều đó.
- Dòng 1347–1350: trong debug, mọi phím khác '0' đều bị từ chối bằng câu “>> Enter '0' to exit debug.” (SmartFarm.ino:189).
- Dòng 1352–1356: gõ '0' để thoát: in “>> Exit debug.” và “Running normally…” (SmartFarm.ino:185–187); ngay vòng loop đầu tiên sau khi thoát, dòng an toàn “nước thấp thì tắt bơm” chạy lại → CHECK xác nhận bơm đã tắt.
- Dòng 1359–1365: ca WiFi mất: ép `mockWifiStatus = WL_DISCONNECTED` rồi gõ '5'; `debugWifi()` in “--- WIFI ---” và gọi `WiFi.reconnect()` khi chưa kết nối (logger.cpp:122–130) → bộ đếm `mockWifiReconnectCalls` đúng bằng 1.
- Dòng 1366–1368: gõ '0' thoát debug rồi trả WiFi về trạng thái kết nối cho các test sau.
- Dòng 1371–1378: hạ mức log xuống `LOG_DEBUG` rồi gọi `logSensors()` và `debugLogger()`: dòng cảm biến định kỳ chỉ in khi mức log cho phép nên phải thấy “[SENSORS]” (logger.cpp:63–67); `debugLogger()` in header mang tag “[DEBUG]”. Sau đó trả mức log về INFO.
- Dòng 1380: `resetState()` trả mọi thứ về mặc định.

### 14.9. D9 + D10 + D11 — chẩn đoán demo, chống spam WiFi, thứ tự khởi động (dòng 1392–1534)

#### D9 — testDemoDiagnostics() (dòng 1392–1458)

**Mục đích:** Chạy trọn trình diễn thật `startDemo()`/`updateDemo()` hai lượt: lượt 1 kiểm tra đúng 15 bước, đúng tên, đúng tổng thời lượng 30.700 ms và dọn sạch cơ cấu; lượt 2 cố tình để cạn nước giữa bước bơm để chứng minh an toàn luôn thắng.

```cpp
static const char *DEMO_STEPS[] = {
  "HD2: Thap sang", "HD3: Den tho PWM", "HD4: Cham LED (bat)", "HD4: Cham LED (tat)",
  "HD5: PIR co nguoi", "HD5: Nguoi roi di", "HD6: Coi xay gio quay", "HD7: Tuoi tu dong",
  "HD8: Bao nuoc can", "HD9: Den tang truong", "HD10: Cho an tu dong", "HD10: Dong cua",
  "HD11: Xem OLED", "HD12: Dieu khien", "HD12: Tat tu xa"
};
static const int DEMO_STEP_COUNT = (int)(sizeof(DEMO_STEPS) / sizeof(DEMO_STEPS[0]));

static void testDemoDiagnostics() {
  printf("D9  demo: step order, duration, OLED names, mid-step safety\n");

  // Lượt 1: chuỗi bước chính xác + tổng thời lượng + tên OLED
  resetState();
  primeSensors(900);
  mockSerialClear();
  unsigned long t0 = mockMillis();
  startDemo();
  CHECK(demoRunning());
  std::vector<std::string> names;
  names.push_back(demoStepName());
  updateOLED(gTemp, gHum, gLight, gSoil, gWater);
  CHECK_STR(mockOledLine(7), names.back().c_str());
  CHECK(strlen(names.back().c_str()) <= 21);

  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
    if (!demoRunning()) break;
    const char *n = demoStepName();
    if (names.back() != n) {
      names.push_back(n);
      updateOLED(gTemp, gHum, gLight, gSoil, gWater);
      CHECK_STR(mockOledLine(7), n);
      CHECK(strlen(n) <= 21);
    }
  }
  unsigned long elapsed = mockMillis() - t0;
  CHECK(!demoRunning());
  CHECK_EQ((int)names.size(), DEMO_STEP_COUNT);
  for (int i = 0; i < DEMO_STEP_COUNT && i < (int)names.size(); i++) {
    CHECK_STR(names[i].c_str(), DEMO_STEPS[i]);
  }
  CHECK(elapsed >= 30700 && elapsed <= 30800);       // tổng thời gian các bước chính xác
  CHECK_EQ(countOccurrences(mockSerialOutput(), "[DEMO] "), DEMO_STEP_COUNT);
  CHECK(has(mockSerialOutput(), "DEMO DONE"));
  CHECK(!relayState);
  CHECK_EQ(mockServoAngle, WINDMILL_MIN_ANGLE);
  CHECK_EQ(currentLedPwm(), 0);
  checkBuzzer(false);

  // Lượt 2: hết nước giữa bước bơm -> an toàn phải ngắt bơm ngay
  resetState();
  primeSensors(900);
  startDemo();
  bool dropped = false, violated = false, sawPumpOn = false;
  cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    if (!dropped && strstr(demoStepName(), "HD7")) {
      dropped = true;
      gWater = 50;                                     // bồn cạn giữa bước
      mockAdc[WATER_PIN] = INVERT_WATER ? 4045 : 50;
      lastSensorRead = mockMillis();
    }
    simLoop();
    if (relayState) sawPumpOn = true;
    if (relayState && gWater < (int)waterThresholdLow) violated = true;
  }
  CHECK(dropped);
  CHECK(sawPumpOn);                                    // bơm chạy khi nước còn ổn
  CHECK(!violated);                                    // sau đó không bao giờ chạy khô
  CHECK(!demoRunning());
  CHECK(!relayState);
}
```

- Dòng 1384–1390: `DEMO_STEPS` là danh sách 15 tên bước viết tay trong test để đối chiếu với tên do firmware thật trả ra; `DEMO_STEP_COUNT` tự tính bằng 15.
- Dòng 1392–1393: khai báo hàm test; câu `printf` chỉ để in tên test.
- Dòng 1396–1397: `resetState()` trả mọi thứ về mặc định; `primeSensors(900)` đổ đầy bộ lọc trung bình động (đọc 5 lần) và đặt mực nước 900.
- Dòng 1399–1401: ghi mốc thời gian `t0`, gọi `startDemo()` thật (demo.cpp:79) rồi xác nhận demo đang chạy.
- Dòng 1402–1405: lưu tên bước đầu tiên, vẽ OLED rồi CHECK dòng 7 hiển thị đúng tên bước — khi demo chạy, dòng trạng thái cuối màn hình thuộc về demo (oled.cpp:68–69).
- Dòng 1406: tên bước phải ≤ 21 ký tự; tên dài nhất “HD6: Coi xay gio quay” đúng 21 ký tự nên vừa màn.
- Dòng 1408–1420: vòng lặp mô phỏng thời gian: mỗi vòng đẩy đồng hồ 10 ms rồi gọi `simLoop()` (bản sao phần điều khiển của loop thật, không HTTP/OLED — test_main.cpp:144–160). Khi tên bước đổi, ghi lại tên mới, vẽ lại OLED và CHECK dòng 7 + giới hạn 21 ký tự. `cap` 120.000 ms chỉ là lưới an toàn chống treo (demo thật dài ~30,7 s).
- Dòng 1421–1423: đo tổng thời gian; xác nhận demo đã dừng; số tên ghi nhận đúng bằng 15.
- Dòng 1424–1426: so từng tên với `DEMO_STEPS` — đúng thứ tự các hoạt động HD2 → HD12.
- Dòng 1427: `elapsed` nằm trong 30.700–30.800 ms. Vì sao đúng 30.700? Cộng 15 khoảng thời gian ở demo.cpp:23–37: 2000+4000+1200+1000+2500+1000+4000+2500+1000+3000+3000+800+2500+1200+1000 = 30.700 ms. Cận trên 30.800 chừa sai số tối đa một nhịp 10 ms khi phát hiện chuyển bước.
- Dòng 1428: đếm log “[DEMO] ” đúng 15 lần — mỗi bước in 1 dòng lúc bắt đầu (bước đầu ở `startDemo()`, các bước sau ở `updateDemo()`).
- Dòng 1429: cuối demo phải có dòng “DEMO DONE”.
- Dòng 1430–1433: demo kết thúc phải dọn sạch: bơm tắt, servo về `WINDMILL_MIN_ANGLE` (mặc định 0 — config.h:83), PWM đèn xanh bằng 0, còi tắt (demo.cpp:112–123).
- Dòng 1436–1440: lượt 2 tái lập môi trường rồi khởi động demo; thêm 3 cờ theo dõi và lưới an toàn 120 s.
- Dòng 1443–1448: ngay khi bước hiện tại là “HD7: Tuoi tu dong”, lần đầu tiên sẽ hạ mực nước xuống 50, cập nhật ADC thô cho khớp (đảo cực nếu INVERT_WATER) và dời `lastSensorRead` để chu kỳ đọc cảm biến không ghi đè ngay.
- Dòng 1449–1451: chạy `simLoop()` rồi ghi nhận: `sawPumpOn` bật khi bơm từng chạy; `violated` bật nếu bắt gặp bơm chạy trong lúc nước dưới ngưỡng.
- Vì sao `violated` luôn sai (không bao giờ chạy khô): bơm chỉ được bật đúng lúc bắt đầu bước HD7, khi nước còn 900; vòng kế tiếp nước đã cạn, dòng an toàn ở đầu `simLoop()` (dòng 152: nước thấp + bơm đang chạy → tắt bơm ngay) cắt bơm trước khi bất kỳ nhánh nào kịp bật lại.
- Dòng 1453–1457: CHECK đủ 4 điều: đã hạ nước giữa bước; bơm từng chạy khi nước còn ổn; chưa từng chạy khô; demo vẫn kết thúc bình thường và bơm tắt khi kết thúc.

#### D10 — testWifiThrottle() (dòng 1461–1504)

**Mục đích:** Kiểm tra bộ đếm chống spam của `manageWiFi()`: chỉ thử kết nối lại đúng mỗi 10.000 ms, chỉ ghi log “WiFi lost” một lần cho mỗi đợt mất mạng, và ghi “WiFi reconnected” một lần khi nối lại.

```cpp
static void testWifiThrottle() {
  printf("D10 wifi: reconnect throttle + counters\n");
  mockWifiStatus = WL_CONNECTED;
  const int c0 = mockWifiBeginCalls;
  loop();                                              // reset cờ đã cảnh báo
  CHECK_EQ(mockWifiBeginCalls, c0);                    // đã kết nối: không thử kết nối lại

  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis();
  const int b0 = mockWifiBeginCalls, d0 = mockWifiDisconnectCalls;
  mockSerialClear();

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                    // chưa đến hạn
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);                // đến hạn đúng tại khoảng thời gian
  CHECK_EQ(mockWifiDisconnectCalls, d0 + 1);
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);

  mockAdvance(WIFI_CHECK_INTERVAL - 1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // thử lại mỗi 10 s
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);  // nhưng chỉ ghi log một lần

  mockWifiStatus = WL_CONNECTED;
  mockSerialClear();
  loop();
  CHECK(has(mockSerialOutput(), "WiFi reconnected"));
  CHECK_EQ(mockWifiBeginCalls, b0 + 2);                // không thử thêm

  // Một lần mất mạng mới lại ghi log (cờ cảnh báo đã được reset bởi lần kết nối lại)
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = mockMillis() - WIFI_CHECK_INTERVAL - 1;
  mockSerialClear();
  loop();
  CHECK_EQ(countOccurrences(mockSerialOutput(), "WiFi lost"), 1);
  mockWifiStatus = WL_CONNECTED;
  loop();
}
```

- Dòng 1461–1462: khai báo hàm test; câu `printf` chỉ để in tên test.
- Dòng 1463–1466: đặt WiFi đang kết nối; lấy mốc `c0` rồi gọi `loop()` — `manageWiFi()` thấy kết nối thì chỉ cập nhật mốc thời gian và xóa cờ đã cảnh báo, không gọi `WiFi.begin()` (wifi.cpp:13–20). CHECK xác nhận không tốn lượt kết nối nào.
- Dòng 1468–1471: ép mất WiFi; đồng bộ `lastWifiCheck` với đồng hồ giả lập; lưu mốc `b0`/`d0` và xóa output serial.
- Dòng 1473–1475: đẩy đồng hồ 9.999 ms (`WIFI_CHECK_INTERVAL − 1` = 10.000 − 1) rồi loop: chưa đến hạn nên không thử lại.
- Dòng 1476–1478: đẩy thêm đúng 1 ms → vừa tròn 10.000 ms, điều kiện `now − lastWifiCheck >= WIFI_CHECK_INTERVAL` (dùng phép trừ chống tràn) đúng → thử kết nối lại, `WiFi.begin()` tăng đúng 1.
- Dòng 1479: mỗi lần thử lại đều `WiFi.disconnect()` trước cho sạch trạng thái → bộ đếm disconnect cũng +1.
- Dòng 1480: thông báo “WiFi lost” xuất hiện đúng 1 lần — cờ tĩnh `sWarned` (wifi.cpp:8) chỉ cho in ở lần mất kết nối đầu.
- Dòng 1482–1487: lặp lại chu kỳ đúng 10.000 ms → lần thử thứ hai; “thử lại mỗi 10 giây” là vì `lastWifiCheck` được cập nhật sau mỗi lần thử.
- Dòng 1488: vẫn chỉ 1 dòng “WiFi lost” dù đã thử 2 lần — chống spam serial.
- Dòng 1490–1493: WiFi kết nối lại → in “WiFi reconnected” đúng một lần khi cờ cảnh báo được hạ (wifi.cpp:14–17).
- Dòng 1494: đã kết nối thì không gọi `WiFi.begin()` thêm.
- Dòng 1497–1501: kịch bản mất mạng lần mới: lùi mốc `lastWifiCheck` về quá khứ để chắc chắn quá hạn; lần này log “WiFi lost” lại xuất hiện 1 lần — chứng minh cờ cảnh báo đã được reset ở lần kết nối lại trước đó.
- Dòng 1502–1503: trả WiFi về kết nối để dọn trạng thái (vòng loop này cũng in “WiFi reconnected” nhưng test không kiểm tra).

#### D11 — testBootOrder() (dòng 1507–1532)

**Mục đích:** Chạy `setup()` thật trên “chip mới tinh” và soi log thứ tự gọi chân: mức an toàn phải được ghi vào chốt ra TRƯỚC khi chuyển chân sang OUTPUT (chống glitch), kèm kiểm tra watchdog và đủ 14 route.

```cpp
static void testBootOrder() {
  printf("D11 boot: output latch before pinMode + WDT\n");
  mockReset();
  mockServerReset();
  mockIoLogClear();
  setup();
  checkRelayPin(false);                                // bơm TẮT khi boot

  const char *log = mockIoLog();
  const char *wRelay = strstr(log, "W:26=");
  const char *mRelay = strstr(log, "M:26;");
  CHECK(wRelay && mRelay);
  CHECK(wRelay < mRelay);                              // ghi chốt trước

  const char *wFan = strstr(log, "W:14=");
  const char *mFan = strstr(log, "M:14;");
  CHECK(wFan && mFan && wFan < mFan);

  const char *wBuzz = strstr(log, "W:25=");
  const char *mBuzz = strstr(log, "M:25;");
  CHECK(wBuzz && mBuzz && wBuzz < mBuzz);

  CHECK(mockWdtInitCalls >= 1);
  CHECK_EQ(mockWdtAddCalls, 1);
  CHECK_EQ(mockServerRouteCount(), 14);
}
```

- Dòng 1507–1508: khai báo hàm test; câu `printf` chỉ để in tên test.
- Dòng 1509: `mockReset()` xóa toàn bộ phần cứng giả lập, bộ đếm, và cả bảng “flash” giả lập `sPrefs` (mock.cpp:355) → chip “mới tinh”, không còn cài đặt cũ.
- Dòng 1510: `mockServerReset()` xóa danh sách route để chắc chắn đếm lại từ 0.
- Dòng 1511: `mockIoLogClear()` xóa log thứ tự gọi chân.
- Dòng 1512: `setup()` thật chạy toàn bộ quá trình khởi động.
- Dòng 1513: `checkRelayPin(false)` — chân bơm phải ở mức TẮT (tùy biến thể active-low/active-high mà helper so mức HIGH/LOW tương ứng).
- Dòng 1515–1519: lấy log IO rồi tìm “W:26=” (lượt ghi mức bằng `digitalWrite`) và “M:26;” (lượt đổi chế độ chân bằng `pinMode`) của chân 26 = RELAY_PIN (config.h:17). Mock ghi log dạng `W:<chân>=<mức>;` và `M:<chân>;` (mock.cpp:56–61). CHECK khẳng định cả hai đều có và vị trí ghi mức đứng TRƯỚC vị trí pinMode.
- Dòng 1521–1523: tương tự cho quạt, chân 14.
- Dòng 1525–1527: tương tự cho còi, chân 25.
- Vì sao kiểm tra thứ tự “ghi mức trước pinMode”: nếu gọi `pinMode(OUTPUT)` trước, chân có thể tạm thời phát mức không mong muốn trong lúc khởi động — với relay kích mức thấp, bơm sẽ “chớp” một cái. Ghi trước mức an toàn vào chốt ra rồi mới chuyển chân sang OUTPUT sẽ triệt hiện tượng glitch đó (xem setup() SmartFarm.ino:81–88).
- Dòng 1529: watchdog được khởi tạo ít nhất 1 lần — setup gọi `esp_task_wdt_init`, bản core 3.x còn gọi `esp_task_wdt_deinit` trước (SmartFarm.ino:66–76).
- Dòng 1530: `esp_task_wdt_add(NULL)` đúng 1 lần (SmartFarm.ino:77) → task loop được bảo vệ và được “nuôi” ở đầu mỗi vòng loop.
- Dòng 1531: setup đăng ký đúng 14 route (SmartFarm.ino:140–153) — mock đếm được vì ghi lại từng lời gọi `server.on`.

### 14.10. D12 — bền vững sau khi mất điện (dòng 1535–1689)

#### D12 — testRebootPersistence() (dòng 1535–1563)

**Mục đích:** Chỉnh ngưỡng và các cờ tự động qua HTTP, rồi mô phỏng một chu kỳ mất điện — chứng minh cài đặt nằm trong “flash” sống sót, còn trạng thái cơ cấu chỉ trong RAM bị mất sạch.

```cpp
static void testRebootPersistence() {
  printf("D12 settings: power-cycle persistence\n");
  invoke("/threshold", "soilLow", "1234");
  CHECK_NEAR(soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(soilThresholdHigh, 1284, 0.01);
  invoke("/auto", "id", "pump", "on", "false");
  invoke("/auto", "id", "breathe", "on", "true");
  invoke("/toggle", "id", "fan");
  CHECK(!pumpAuto);
  CHECK(breatheAuto);
  CHECK(!fanAuto);
  CHECK(fanState);

  // chu kỳ nguồn: flash giữ prefs, trạng thái chỉ trong RAM bị mất
  mockServerReset();
  fanState = relayState = windmillOn = blueLedState = breatheLedState = false;
  rgbR = rgbG = rgbB = 0;
  setup();

  CHECK_EQ(mockServerRouteCount(), 14);
  CHECK_NEAR(soilThresholdLow, 1234, 0.01);
  CHECK_NEAR(soilThresholdHigh, 1284, 0.01);
  CHECK(!pumpAuto);
  CHECK(!fanAuto);
  CHECK(breatheAuto);
  CHECK(breatheLedState);                              // setup bật lại breathing
  CHECK(!fanState);                                    // trạng thái runtime được reset
  CHECK_EQ(rgbR, 0);                                   // RGB thủ công không được lưu bền
}
```

- Dòng 1535–1536: khai báo hàm test; câu `printf` chỉ để in tên test.
- Dòng 1537: gọi handler thật `/threshold` với `soilLow = 1234` qua helper `invoke()` (helper nạp tham số rồi gọi `mockServerInvoke` và kiểm tra route tồn tại).
- Dòng 1538: ngưỡng dưới đất được đặt đúng 1234 (so `CHECK_NEAR` vì là số thực).
- Dòng 1539: ngưỡng trên đất tự thành 1284 = 1234 + 50 — vì `handleThreshold()` ép `soilHigh = soilLow + 50` khi phát hiện `high ≤ low` (webserver.cpp:448–450).
- Dòng 1540: tắt tự động bơm qua `/auto?...on=false` → `pumpAuto = false`.
- Dòng 1541: bật tự động chế độ thở → `breatheAuto = true` và handler còn bật luôn đèn thở.
- Dòng 1542: `/toggle` thiết bị “fan”: tắt tự động quạt (`fanAuto = false`) rồi đảo trạng thái quạt đang tắt thành bật (webserver.cpp:311–333).
- Dòng 1543–1546: CHECK đủ 4 trạng thái trước khi “mất điện”.
- Mỗi handler ở trên đều gọi `saveSettings()` (webserver.cpp:331/312), tức “ghi vào flash”: bảng flash giả lập `sPrefs` được cập nhật qua `mockPrefPut()` (mock.cpp:331, Preferences.h:12–13). `saveSettings()` chỉ lưu ngưỡng + 5 cờ tự động, KHÔNG lưu trạng thái bơm/quạt/đèn/RGB (settings.cpp:6–21).
- Dòng 1548: chú thích nói rõ mô hình: flash giữ prefs, trạng thái chỉ trong RAM bị mất.
- Dòng 1549: `mockServerReset()` đóng vai “khởi động lại chip”: nó chỉ xóa route và tham số request tạm thời (mock.cpp:235–241), KHÔNG đụng tới `sPrefs` — khác hẳn `mockReset()` ở D11 vốn xóa cả `sPrefs` (mock.cpp:355). Vì thế “flash” vẫn nguyên.
- Dòng 1550–1551: tự tay xóa các biến trạng thái chỉ sống trong RAM (bơm, quạt, cối xay, 2 chế độ đèn xanh, RGB) — tương đương cúp điện làm RAM mất sạch.
- Dòng 1552: `setup()` chạy lại như lần cấp điện đầu: đăng ký lại 14 route và gọi `loadSettings()` đọc cài đặt từ “flash” (Preferences mock → `mockPrefGet`, Preferences.h:14–16).
- Dòng 1554: đủ 14 route được đăng ký lại sau khi khởi động.
- Dòng 1555–1556: hai ngưỡng đất 1234/1284 vẫn còn nguyên — bằng chứng sống sót qua “mất điện”.
- Dòng 1557–1559: các cờ tự động bơm/quạt/thở đều khôi phục đúng: bơm không tự động, quạt không tự động, thở tự động.
- Dòng 1560: `breatheLedState` đang bật vì setup có dòng `if (breatheAuto) setBreatheLed(true)` (SmartFarm.ino:119) — hiệu ứng thở đã lưu được khôi phục khi boot.
- Dòng 1561: `fanState` vẫn tắt dù có cài đặt flash — trạng thái runtime không được lưu bền, phải chờ automation hoặc người dùng bật lại.
- Dòng 1562: màu RGB đặt thủ công cũng không sống sót (các khóa RGB không có trong `saveSettings()`) — sau khi boot lại, RGB vẫn tắt.

### 14.11. D13 + D14 — fuzz 90.000 bước và tràn millis() (dòng 1690–1796)

Hai test cuối của nhóm "deep": **D13** dùng fuzz/soak để "tra tấn" toàn bộ logic điều khiển bằng 90.000 bước ngẫu nhiên có seed, còn **D14** kiểm tra an toàn khi `millis()` của ESP32 bị tràn số (wrap-around) sau khoảng 49,7 ngày chạy liên tục.

#### D13 — testFuzzSoak() (dòng 1690–1707)

**Mục đích:** Chạy 90.000 vòng `loop()` với môi trường, HTTP request, lệnh serial và thao tác cơ cấu ngẫu nhiên (3 seed × 30.000 bước), sau **mỗi** vòng kiểm tra các "safety invariant" (bất biến an toàn) để phát hiện lỗi hiếm gặp mà test thủ công không chạm tới.

```cpp
static void testFuzzSoak() {
  printf("D13 fuzz: seeded soak with safety invariants\n");
  resetState();
  mockWifiStatus = WL_CONNECTED;
  mockServerReset();
  setup();                                   // route + giá trị mặc định từ "flash"

  fuzzRun(1,    30000);
  fuzzRun(42,   30000);
  fuzzRun(2026, 30000);

  // ổn định: để demo đang chạy kết thúc, rồi kiểm tra trạng thái yên tĩnh
  stopFeed();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); loop(); }
  CHECK(!demoRunning());
  mockSerialClear();
}
```

- Dòng 1690: khai báo hàm test (chỉ dùng trong file này).
- Dòng 1691: in tên test để đọc log biết đang chạy mục nào.
- Dòng 1692: `resetState()` đưa mọi biến toàn cục về mặc định (ngưỡng, trạng thái relay, cờ auto... — xem định nghĩa ở dòng 116).
- Dòng 1693: giả lập WiFi đang kết nối để `loop()` không mất thời gian thử kết nối lại.
- Dòng 1694: `mockServerReset()` xóa các route/tham số HTTP đã đăng ký trên web server giả.
- Dòng 1695: `setup()` chạy đúng hàm boot thật: đăng ký 14 route, nạp settings từ "flash" giả, bật breathing nếu `breatheAuto`.
- Dòng 1697–1699: ba lượt `fuzzRun` với seed **1**, **42**, **2026**, mỗi lượt 30.000 vòng → đúng 90.000 bước. Dùng nhiều seed để dải dữ liệu ngẫu nhiên đa dạng hơn, và seed cố định nên **kết quả lặp lại được** (chạy lại luôn ra cùng chuỗi).
- Dòng 1702: `stopFeed()` đóng máng ăn trước khi chờ ổn định.
- Dòng 1703: đặt trần thời gian mô phỏng 120.000 ms, tránh vòng lặp chờ treo vô hạn nếu demo không bao giờ kết thúc.
- Dòng 1704: mỗi vòng tiến đồng hồ 10 ms rồi gọi `loop()` cho đến khi demo kết thúc. Phép `mockMillis() < cap` vẫn đúng khi đồng hồ tràn nhờ luật trừ số unsigned (xem D14).
- Dòng 1705: `CHECK(!demoRunning())` — sau khi chờ, demo **buộc phải** kết thúc sạch.
- Dòng 1706: dọn log serial để không lẫn sang test sau.

**Fuzz và soak là gì?**
- *fuzz* = dùng dữ liệu/hành động ngẫu nhiên làm đầu vào để tìm lỗi; *soak* = chạy "ngâm" thật lâu, thật nhiều vòng để lỗi hiếm tự lộ ra.
- *seed* = hạt giống của bộ sinh số ngẫu nhiên `std::mt19937` (dòng 1595). Cùng seed ⇒ cùng chuỗi số ngẫu nhiên ⇒ test tái lập được y nguyên.
- *invariant* = điều kiện luôn phải đúng, ví dụ "bơm không bao giờ chạy khi hết nước". Sau **mỗi** lần `loop()`, `checkFuzzInvariants()` kiểm tra lại toàn bộ.

**Bộ máy fuzz — fuzzRun(seed, iters) (dòng 1594–1688):** mỗi vòng bốc ngẫu nhiên `rng() % 10` để chọn 1 trong 10 loại hành động (dòng 1609–1679):

- case 0 (dòng 1610–1624): môi trường ngẫu nhiên — gán nước/đất/ánh sáng/nhiệt/độ ẩm và lật chân TOUCH/PIR.
- case 1 (dòng 1625–1636): gọi HTTP ngẫu nhiên (trong 13 route) với 0–3 tham số ngẫu nhiên; mã trả về chỉ được là 200, 204 hoặc 400. Nếu bản build có `WEB_TOKEN`, dòng 1630 tự gắn header `X-Auth-Token` trước khi gọi nên request không bị 401 oan.
- case 2 (dòng 1637–1642): bơm ký tự serial ngẫu nhiên (`'9'`, `'0'`, `'x'`, `'!'`, `'\n'`, `'\r'`, `' '`).
- case 3 (dòng 1643–1650): bật/tắt thủ công relay, quạt, LED xanh, LED thở.
- case 4 (dòng 1651–1657): điều khiển cặp servo dùng chung — cối xay gió, mở/đóng máng ăn.
- case 5–6 (dòng 1658–1664): `startDemo()` hoặc gọi route `/demo`; case 6 cũng gắn header `X-Auth-Token` khi build có token (dòng 1662).
- case 7 (dòng 1665–1668): lật chân TOUCH và PIR ngẫu nhiên.
- case 8 (dòng 1669–1676): cập nhật ngưỡng qua `/threshold` bằng giá trị ngẫu nhiên (kể cả số âm); gắn header `X-Auth-Token` khi build có token (dòng 1673).
- case 9 (dòng 1677–1678): không làm gì (no-op).

Phần đuôi mỗi vòng lặp (dòng 1681–1686):

```cpp
    mockAdvance(1 + rng() % 250);
    bool demoAtLoopStart = demoRunning();
    loop();
    checkFuzzInvariants(demoAtLoopStart);

    if (i % 1000 == 999) mockSerialClear();   // giới hạn buffer bắt log
```

- Dòng 1681: nhảy đồng hồ 1–250 ms ngẫu nhiên (thời điểm thực tế không bao giờ tròn trịa).
- Dòng 1682: chụp lại trạng thái demo **trước** khi gọi `loop()` (xem giải thích bên dưới).
- Dòng 1683: chạy đúng `loop()` thật của firmware.
- Dòng 1684: kiểm tra các bất biến an toàn sau vòng lặp vừa rồi.
- Dòng 1686: cứ 1000 vòng dọn buffer serial một lần — nếu không, log tích tụ làm test chậm và tốn RAM vô ích.

**Các safety invariant được kiểm tra sau mỗi loop() — checkFuzzInvariants() (dòng 1566–1592):**

```cpp
static void checkFuzzInvariants(bool demoAtLoopStart) {
  // 1. Bơm không bao giờ chạy khi hết nước (safety line + controlPump).
  CHECK(!(gWater < (int)waterThresholdLow && relayState));
  // 2. Mức chân luôn khớp với trạng thái logic.
  CHECK((mockPinLevel[RELAY_PIN] != 0) == (relayState != (bool)RELAY_ACTIVE_LOW));
  CHECK((mockPinLevel[FAN_PIN] != 0) == fanState);
  // 3. Các cơ cấu servo dùng chung không bao giờ chạy cùng lúc.
  CHECK(!(windmillOn && feedRunning()));
  // 4. Các chế độ LED xanh loại trừ lẫn nhau.
  CHECK(!(blueLedState && breatheLedState));
  // 5. Ghi servo và PWM luôn nằm trong dải hợp lệ.
  CHECK(mockServoMinSeen >= 0 && mockServoMaxSeen <= 180);
  CHECK_EQ(mockInvalidPixelWrites, 0);
  CHECK_EQ(mockInvalidPwmWrites, 0);
  // 6. Không còn PWM cũ khi cả hai chế độ LED đều tắt.
  if (!blueLedState && !breatheLedState) CHECK_EQ(currentLedPwm(), 0);
  // 7. Khi còi đang báo động (không phải demo), còi phải thực sự bật.
  // Bỏ qua loop mà demo đang chạy vừa kết thúc: automation (và
  // còi báo) chỉ hoạt động lại ở loop kế tiếp.
  if (!demoRunning() && !demoAtLoopStart && gWater < (int)waterThresholdLow) {
#if BUZZER_PASSIVE
    CHECK(mockBuzzerToneHz > 0);
#else
    CHECK_EQ(mockPinLevel[BUZZER_PIN], HIGH);
#endif
  }
}
```

- Dòng 1568 (invariant 1): bơm **không bao giờ** chạy khô — nước dưới ngưỡng thì relay phải tắt, dù đang ở chế độ thủ công hay demo.
- Dòng 1570–1571 (invariant 2): mức điện thực tế trên chân phải khớp trạng thái logic. Với relay còn tính `RELAY_ACTIVE_LOW` (relay kích mức thấp): chân khác 0 tương đương `relayState != RELAY_ACTIVE_LOW`; quạt thì khớp trực tiếp.
- Dòng 1573 (invariant 3): cối xay gió và máng ăn **loại trừ nhau** vì dùng chung một servo — không bao giờ cùng chạy.
- Dòng 1575 (invariant 4): LED xanh thường và LED thở loại trừ nhau (cùng một bóng LED, hai chế độ không được giẫm chân).
- Dòng 1577–1579 (invariant 5): mọi góc servo ghi ra phải nằm trong 0–180; không được ghi pixel màu hay kênh PWM không hợp lệ.
- Dòng 1581 (invariant 6): khi cả hai chế độ LED đều tắt thì PWM phải về 0, không được để "dư âm" sáng mờ.
- Dòng 1585–1590 (invariant 7): hết nước (ngoài demo) thì còi báo động phải thực sự kêu — kiểm tra tần số `tone()` nếu buzzer loại passive, hoặc mức chân HIGH nếu loại active.

**Vì sao có tham số `demoAtLoopStart`?** Khi demo vừa kết thúc ngay trong `loop()` vừa rồi, firmware chỉ chạy lại các bộ điều khiển tự động (trong đó có còi báo) ở **loop kế tiếp** (xem nhánh `if (demoRunning()) ... else ...` trong `loop()`). Lúc đó `demoRunning()` đã trả về `false`, nhưng còi chưa kịp bật — nếu không có cờ `demoAtLoopStart` (trạng thái demo lúc *bắt đầu* vòng lặp), invariant 7 sẽ báo lỗi giả ở đúng một vòng chuyển giao. Cờ này giúp bỏ qua vòng "giao ca" đó.

#### D14 — testClockWrap() (dòng 1710–1794)

**Mục đích:** Xác nhận mọi bộ đếm thời gian trong firmware vẫn đúng khi `millis()` quay vòng từ `0xFFFFFFFF` về `0` giữa chừng — sự cố hiếm nhưng thật, xảy ra sau ~49,7 ngày chạy liên tục.

**Vì sao tràn `millis()` nguy hiểm?** `millis()` trả về `unsigned long` 32 bit, đếm mili-giây nên tràn sau 2³² ms = 4.294.967.296 ms ≈ **49,71 ngày**. Cách viết sai như `now >= last + interval` sẽ hỏng khi `last + interval` vượt 2³² (tràn thành số nhỏ). Cách viết đúng là phép trừ: `now - last >= interval` — với số unsigned, phép trừ tự "quấn" theo modulo 2³² và luôn cho đúng khoảng thời gian trôi qua, miễn khoảng đo nhỏ hơn 2³².

```cpp
// D14: an toàn wrap-around của millis()
static void testClockWrap() {
  printf("D14 clock: millis() wrap-around\n");
  stopFeed();
  setWindmill(false);

  // A: đọc cảm biến định kỳ xuyên qua mốc wrap
  resetState();
  mockSetMillis(0xFFFFFF00UL);
  mockDhtTemp = 41.0f; mockDhtHum = 33.0f;
  mockAdc[LDR_PIN] = 1500; mockAdc[SOIL_PIN] = 800; mockAdc[WATER_PIN] = 1200;
  lastSensorRead = mockMillis();
  runLoop(5100);                             // vượt mốc wrap tại +256 ms
  CHECK_NEAR(gTemp, 41.0, 0.01);
  CHECK_NEAR(gHum, 33.0, 0.01);

  // B: timer cho ăn trải qua mốc wrap
  mockSetMillis(0xFFFFFFFFUL);
  startFeed();
  CHECK(feedRunning());
  mockAdvance(1);                            // wrap về 0
  mockAdvance(1998);
  controlFeed();
  CHECK(feedRunning());                      // 1999 ms < FEED_DURATION
  mockAdvance(2);
  controlFeed();
  CHECK(!feedRunning());
  CHECK_EQ(mockServoAngle, FEED_CLOSE_ANGLE);

  // C: timer breathing trải qua mốc wrap
  mockSetMillis(0xFFFFFFFFUL);
  setBreatheLed(true);
  setLedPwm(0);
  lastBreatheUpdate = mockMillis();
  mockAdvance(BREATHE_INTERVAL - 1);
  controlBreathe();
  CHECK_EQ(currentLedPwm(), 0);
  mockAdvance(1);
  controlBreathe();
  CHECK_EQ(currentLedPwm(), 5);
  setBreatheLed(false);

  // D: chạm bật/tắt với timestamp lớn
  mockSetMillis(0xF0000000UL);
  mockAdvance(10000);
  gTouch = false; controlTouch();
  lastTouch = mockMillis();
  setBlueLed(false);
  bool before = blueLedState;
  mockAdvance(TOUCH_DEBOUNCE);               // xóa cửa sổ debounce trước
  gTouch = true; controlTouch();
  CHECK(blueLedState != before);
  gTouch = false; controlTouch();
  setBlueLed(false);

  // E: demo đầy đủ phải kết thúc sạch khi mốc wrap xảy ra giữa lượt chạy
  mockSetMillis(0xFFFFFF00UL);
  primeSensors(900);
  startDemo();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) {
    mockAdvance(10);
    simLoop();
  }
  CHECK(!demoRunning());
  CHECK(!relayState);
  CHECK_EQ(currentLedPwm(), 0);

  // F: timer thử lại WiFi trải qua mốc wrap
  mockSetMillis(0xFFFFFF00UL);
  mockWifiStatus = WL_CONNECTED;
  loop();                                            // xóa cờ đã cảnh báo
  int b0 = mockWifiBeginCalls;
  mockWifiStatus = WL_DISCONNECTED;
  lastWifiCheck = 0xFFFFFF00UL;
  mockAdvance(9999);                                  // wrap qua mốc biên
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0);                   // 9999 ms: chưa đến hạn
  mockAdvance(1);
  loop();
  CHECK_EQ(mockWifiBeginCalls, b0 + 1);               // 10000 ms: đến hạn
  mockWifiStatus = WL_CONNECTED;
  loop();

  resetState();
}
```

- Dòng 1712–1713: dọn cơ cấu trước khi test — đóng máng ăn và tắt cối xay gió (fuzz ở D13 có thể để chúng đang chạy, và servo là tài nguyên dùng chung).
- **Phần A — đọc cảm biến định kỳ xuyên mốc wrap (dòng 1715–1723):**
  - Dòng 1716: `resetState()` đưa ngưỡng/cờ về mặc định.
  - Dòng 1717: `mockSetMillis(0xFFFFFF00UL)` — đặt đồng hồ chỉ còn cách mốc tràn **256 ms** (vì 0xFFFFFF00 = 2³² − 256).
  - Dòng 1718–1719: gán giá trị DHT và ADC giả để biết trước kết quả đọc.
  - Dòng 1720: `lastSensorRead = mockMillis()` — mốc đọc gần nhất là "bây giờ" (sát mốc tràn).
  - Dòng 1721: `runLoop(5100)` chạy 5100 ms theo bước 10 ms — quãng này **băng qua** mốc tràn tại +256 ms. Lưu ý chính vòng lặp `while (mockMillis() - start < simMs)` của `runLoop` (dòng 175–181) cũng dựa vào phép trừ unsigned nên tự nó đã là một phép thử wrap.
  - Dòng 1722–1723: nhiệt/ẩm đọc được phải đúng bằng giá trị giả ⇒ bộ đếm `now - lastSensorRead >= SENSOR_INTERVAL` vẫn kích hoạt đúng sau khi tràn.
- **Phần B — timer cho ăn xuyên mốc wrap (dòng 1725–1736):**
  - Dòng 1726–1728: đặt đồng hồ sát mốc tràn, mở máng ăn và xác nhận đang chạy.
  - Dòng 1729: `mockAdvance(1)` — đồng hồ **tràn về 0**; thời gian trôi từ lúc mở = 1 ms.
  - Dòng 1730–1732: cộng thêm 1998 ms → trôi 1999 ms `< FEED_DURATION` (2000 ms) nên máng vẫn mở.
  - Dòng 1733–1735: cộng thêm 2 ms → trôi 2001 ms `≥ 2000 ms` nên `controlFeed()` đóng máng.
  - Dòng 1736: servo phải về đúng `FEED_CLOSE_ANGLE` (0°).
- **Phần C — timer breathing xuyên mốc wrap (dòng 1738–1749):**
  - Dòng 1739–1742: đặt đồng hồ sát tràn, bật LED thở, ép PWM = 0 và mốc cập nhật = bây giờ.
  - Dòng 1743–1745: sau `BREATHE_INTERVAL − 1` = 29 ms, chưa tới nhịp 30 ms nên PWM vẫn 0.
  - Dòng 1746–1748: thêm 1 ms (đủ 30 ms, đã đi qua mốc tràn) → `controlBreathe()` tăng độ sáng một bậc lên PWM 5.
  - Dòng 1749: tắt breathing để phần sau không bị ảnh hưởng.
- **Phần D — chạm bật/tắt với timestamp lớn (dòng 1751–1762):**
  - Dòng 1752–1753: đặt đồng hồ ở `0xF0000000` (≈ 4,02 tỉ ms — mốc thời gian lớn) rồi cộng 10.000 ms.
  - Dòng 1754–1755: xác lập trạng thái chạm sạch và đặt `lastTouch = bây giờ`, tạo cửa sổ debounce mới.
  - Dòng 1758: cộng đúng `TOUCH_DEBOUNCE` (300 ms) để cửa sổ debounce cũ hết hạn trước khi thử.
  - Dòng 1759–1760: chạm mới phải đổi được trạng thái LED xanh — chứng minh công thức `now - lastTouch >= TOUCH_DEBOUNCE` đúng với mốc thời gian lớn.
- **Phần E — demo đầy đủ xuyên mốc wrap (dòng 1764–1775):**
  - Dòng 1765–1767: bắt đầu demo ở 256 ms trước mốc tràn, với nước đầy (`primeSensors(900)`) để demo chạy trọn kịch bản.
  - Dòng 1768–1772: chờ tối đa 120.000 ms, mỗi bước 10 ms gọi `simLoop()` (đúng phần điều khiển của `loop()`).
  - Dòng 1773–1775: demo phải kết thúc, relay tắt, PWM về 0 — "kết thúc sạch" dù mốc tràn rơi vào giữa lượt chạy.
- **Phần F — timer thử lại WiFi xuyên mốc wrap (dòng 1777–1791):**
  - Dòng 1778–1780: đặt đồng hồ sát tràn và giả lập đang kết nối; gọi `loop()` để `manageWiFi()` xóa cờ "đã cảnh báo" (`sWarned` trong `src/wifi.cpp:8`).
  - Dòng 1781–1783: ghi nhớ số lần gọi `WiFi.begin()`, chuyển sang mất kết nối, đặt `lastWifiCheck = 0xFFFFFF00UL`.
  - Dòng 1784–1786: cộng 9999 ms (đã băng qua mốc tràn), trôi đúng 9999 ms `< WIFI_CHECK_INTERVAL` (10.000 ms) nên **chưa** thử kết nối lại.
  - Dòng 1787–1789: thêm 1 ms → đúng hạn 10.000 ms, `WiFi.begin()` được gọi thêm đúng 1 lần.
  - Dòng 1790–1791: trả về trạng thái kết nối rồi gọi `loop()` một nhịp để firmware ổn định lại cờ cảnh báo.
- Dòng 1793: `resetState()` trả toàn bộ trạng thái về mặc định cho test I11 phía sau.

### 14.12. T16 + T17 + I11 + main() — quyền sở hữu đèn của PIR, khôi phục đèn thở, boot suy giảm, hàm main (dòng 1797–2033)

Bốn phần cuối file: hai test hành vi (T16, T17), một test boot suy giảm (I11) và hàm `main()` điều phối toàn bộ bộ test.

#### T16 — testAlertLedOwnership() (dòng 1797–1844)

**Mục đích:** Xác nhận "quyền sở hữu" LED xanh: PIR chỉ được tắt LED khi chính nó là thứ đã bật LED; đèn bật thủ công không bị người đi qua làm tắt; sau cảnh báo, chế độ đèn thở được khôi phục nếu đang bật auto.

```cpp
// T16: quyền sở hữu LED cảnh báo của PIR + khôi phục breathing (hoạt động 3/5)
static void testAlertLedOwnership() {
  printf("T16 alerts: PIR LED ownership + breathing restore\n");

  // Có chuyển động khi đang breathing: cảnh báo chiếm LED; breathing tiếp tục sau đó
  resetState();
  gPir = false; controlAlerts();          // xóa mọi trạng thái cảnh báo đang chờ
  setBreatheAuto(true);
  setBreatheLed(true);
  CHECK(breatheLedState);
  gPir = true;
  controlAlerts();
  CHECK(blueLedState);
  CHECK(!breatheLedState);                // cảnh báo đã chiếm LED
  gPir = false;
  controlAlerts();
  CHECK(!blueLedState);
  CHECK(breatheLedState);                 // breathing tự chọn được bật lại
  CHECK(breatheAuto);
  setBreatheAuto(false);
  setBreatheLed(false);

  // LED bật thủ công không bị ảnh hưởng khi có người đi qua (PIR không sở hữu nó)
  resetState();
  gPir = false; controlAlerts();
  setBlueLed(true);
  gPir = true;  controlAlerts();
  CHECK(blueLedState);
  gPir = false; controlAlerts();
  CHECK(blueLedState);
  setBlueLed(false);

  // Không có breathing auto, cảnh báo chỉ tắt LED trở lại
  resetState();
  gPir = false; controlAlerts();
  gPir = true;  controlAlerts();
  CHECK(blueLedState);
  gPir = false; controlAlerts();
  CHECK(!blueLedState);
  CHECK(!breatheLedState);

  // Phòng thủ: giá trị ramp cũ được xóa khi breathing tắt
  resetState();
  breatheBrightness = 100;
  setLedPwm(222);
  controlBreathe();
  CHECK_EQ(breatheBrightness, 0);
  CHECK_EQ(currentLedPwm(), 0);
}
```

- Dòng 1798: in tên test.
- **Kịch bản 1 — đang thở mà có người đi qua (dòng 1801–1816):**
  - Dòng 1801–1802: reset trạng thái, gọi `controlAlerts()` với `gPir = false` để xóa các cờ cảnh báo còn treo từ trước.
  - Dòng 1803–1805: bật đèn thở tự động và đèn thở; xác nhận `breatheLedState` đang bật.
  - Dòng 1806–1807: giả lập phát hiện chuyển động rồi gọi `controlAlerts()` (cạnh lên của PIR).
  - Dòng 1808: LED xanh phải bật — cảnh báo đã **chiếm** LED.
  - Dòng 1809: đèn thở phải tắt — nhường LED cho cảnh báo (LED dùng chung nên hai chế độ loại trừ nhau).
  - Dòng 1810–1811: hết chuyển động, gọi lại `controlAlerts()`.
  - Dòng 1812: LED xanh tắt — vì chính PIR là thứ đã bật nó (nhớ qua cờ `sPirLitLed`).
  - Dòng 1813–1814: đèn thở **tự bật lại** vì `breatheAuto` vẫn đang bật; cờ `breatheAuto` giữ nguyên.
  - Dòng 1815–1816: dọn dẹp để sang kịch bản sau.
- **Kịch bản 2 — đèn bật thủ công (dòng 1818–1826):** bật `setBlueLed(true)` trước, rồi PIR lên/xuống. Cả hai lần kiểm tra đèn vẫn sáng: vì lúc phát hiện, đèn đã sáng nên `sPirLitLed = false` (PIR không nhận công) và **không được quyền** tắt đèn của người dùng.
- **Kịch bản 3 — không bật breathing auto (dòng 1828–1835):** PIR bật đèn khi có người; hết người thì chỉ tắt đèn trở lại, không bật đèn thở (`CHECK(!blueLedState)` và `CHECK(!breatheLedState)`).
- **Kịch bản 4 — phòng thủ giá trị ramp cũ (dòng 1837–1843):** gán giả `breatheBrightness = 100` và PWM = 222, gọi `controlBreathe()` khi breathing đang tắt → hàm phải xóa ramp về 0 và kéo PWM về 0, tránh "dư âm" sáng mờ còn sót lại.

**`sPirLitLed` là gì?** Đây là biến `static` trong `src/control.cpp:11`:

```cpp
static bool sPirLitLed = false;   // true = cảnh báo PIR là thứ đã bật LED
```

Khi phát hiện chuyển động (cạnh lên), `controlAlerts()` ghi `sPirLitLed = !blueLedState` (`src/control.cpp:147`) — chỉ đánh dấu "PIR sở hữu LED" nếu **trước đó đèn đang tắt**. Khi hết chuyển động, chỉ khi `sPirLitLed` đúng thì PIR mới được tắt LED; và nếu `breatheAuto` đang bật thì bật lại đèn thở (`src/control.cpp:152–155`). Nhờ vậy đèn đang sáng do thủ công/tự động/đang thở không bị người đi qua tắt mất.

#### T17 — testDemoBreathingRestore() (dòng 1847–1862)

**Mục đích:** Sau khi demo bài học kết thúc, chế độ đèn thở đang bật auto phải được khôi phục (demo đã tạm chiếm LED trong lúc trình diễn).

```cpp
// T17: demo hoạt động khôi phục breathing tự chọn khi kết thúc
static void testDemoBreathingRestore() {
  printf("T17 demo: breathing restored after the show\n");
  resetState();
  primeSensors(900);
  setBreatheAuto(true);
  setBreatheLed(true);
  startDemo();
  unsigned long cap = mockMillis() + 120000;
  while (demoRunning() && mockMillis() < cap) { mockAdvance(10); simLoop(); }
  CHECK(!demoRunning());
  CHECK(breatheAuto);
  CHECK(breatheLedState);                 // breathing bật lại sau DEMO DONE
  CHECK(!blueLedState);
  setBreatheAuto(false);
  setBreatheLed(false);
}
```

- Dòng 1848: in tên test.
- Dòng 1849–1850: reset trạng thái và "mồi" cảm biến với nước đầy (900) để demo chạy hết kịch bản không bị ngắt vì an toàn nước.
- Dòng 1851–1852: bật đèn thở auto + đèn thở trước khi demo bắt đầu (mô phỏng người dùng đang để chế độ thở).
- Dòng 1853: `startDemo()` — demo chiếm LED cho các hiệu ứng của bài học.
- Dòng 1854–1855: chờ demo kết thúc, tối đa 120.000 ms mô phỏng.
- Dòng 1856: demo phải dừng.
- Dòng 1857: cờ `breatheAuto` vẫn phải giữ nguyên (demo không được tắt ý định của người dùng).
- Dòng 1858: `breatheLedState` phải bật lại — đúng như `demo.cpp:121`: sau "DEMO DONE", nếu `breatheAuto` thì bật lại breathing.
- Dòng 1859: LED xanh thường không được bật (không có tranh chấp LED sau demo).
- Dòng 1860–1861: dọn dẹp.

#### I11 — testBootDegraded() (dòng 1952–1971)

**Mục đích:** Boot trong tình trạng suy giảm — **không có màn hình OLED** và **không kết nối được WiFi** — firmware vẫn phải ghi đúng mã lỗi, đăng ký đủ route web, nuôi watchdog trong lúc chờ và không tự chạy demo.

```cpp
// I11: boot suy giảm - thiếu OLED và timeout kết nối WiFi
static void testBootDegraded() {
  printf("I11 boot: missing OLED + WiFi timeout\n");
  resetState();
  mockServerReset();
  mockDisplayBeginOk = false;
  mockWifiStatus = WL_DISCONNECTED;
  mockSerialClear();
  setup();

  CHECK_EQ(mockServerRouteCount(), 14);              // route vẫn được đăng ký
  CHECK(has(mockSerialOutput(), "MA LOI: 101"));     // không tìm thấy OLED
  CHECK(has(mockSerialOutput(), "MA LOI: 102"));     // WiFi timeout
  CHECK(mockWdtResetCalls >= 100);                   // watchdog được feed trong lúc chờ
  CHECK(mockWifiBeginCalls >= 1);
  CHECK(!demoRunning());

  mockDisplayBeginOk = true;
  mockWifiStatus = WL_CONNECTED;
  resetState();
}
```

- Dòng 1953: in tên test.
- Dòng 1954–1955: reset trạng thái và web server giả.
- Dòng 1956: `mockDisplayBeginOk = false` — giả lập `display.begin()` trả về thất bại, tức phần cứng không có OLED.
- Dòng 1957: `mockWifiStatus = WL_DISCONNECTED` — giả lập WiFi không bao giờ kết nối, buộc nhánh chờ phải hết hạn.
- Dòng 1958: `mockSerialClear()` để log bắt được chỉ chứa thông báo của lần boot này.
- Dòng 1959: `setup()` — chạy đúng hàm boot thật trong điều kiện suy giảm.
- Dòng 1961: dù thiếu OLED/WiFi, vẫn phải đăng ký đủ **14 route** của web server.
- Dòng 1962: log phải chứa `MA LOI: 101` — mã lỗi "OLED not found" (`SmartFarm.ino:111`).
- Dòng 1963: log phải chứa `MA LOI: 102` — mã lỗi "WiFi timeout" (`SmartFarm.ino:136`): chờ tối đa `WIFI_CONNECT_TIMEOUT` = 30.000 ms rồi chạy tiếp, `manageWiFi()` sẽ thử lại nền.
- Dòng 1964: `mockWdtResetCalls >= 100` — trong vòng chờ WiFi blocking, mỗi 250 ms firmware gọi `esp_task_wdt_reset()` (`SmartFarm.ino:127`), tức khoảng 120 lần trên 30 giây; nếu không nuôi watchdog, chip sẽ bị reset giữa lúc boot.
- Dòng 1965: đã có ít nhất một lần gọi `WiFi.begin()`.
- Dòng 1966: boot xong không tự ý chạy demo.
- Dòng 1968–1970: khôi phục cờ mock (OLED OK, WiFi kết nối) và reset trạng thái.

**Vì sao I11 nằm cuối `main()`?** `setup()` là một lần boot đầy đủ, nó đụng và để lại nhiều trạng thái/counter toàn cục: cờ mock WiFi/OLED, `mockWifiBeginCalls`, `mockWdtResetCalls`, route của server giả... Trong khi `resetState()` không dọn hết mọi thứ (các biến `static` bên trong `src/` như `sPirLitLed`, `sMotionAlarm`, `sWarned` không bị nó chạm tới). Đặt I11 cuối cùng đảm bảo "cú boot suy giảm" này không làm nhiễu bất kỳ test nào chạy sau nó — phía sau chỉ còn in tổng kết và `return`.

#### main() (dòng 1977–2033)

**Mục đích:** Điểm vào của toàn bộ bộ test — in banner cấu hình biên dịch, gọi lần lượt các test theo thứ tự có chủ đích (unit → biên → integration → deep), in tổng kết và trả exit code.

```cpp
int main() {
  printf("=== Smart Farm deep test suite (core %d, invert L/S/W %d/%d/%d, relayLO %d, buzzPassive %d) ===\n",
         ESP_ARDUINO_VERSION_MAJOR, INVERT_LIGHT, INVERT_SOIL, INVERT_WATER,
         RELAY_ACTIVE_LOW, BUZZER_PASSIVE);

  testSensorsAverage();
  testDhtFailover();
  testPumpHysteresis();
  testPumpLowWaterSafety();
  testFanHysteresis();
  testLightPirAlerts();
  testBreatheAndLedModes();
  testTouchToggle();
  testRgbPlant();
  testBuzzerAlarm();
  testWindmillSweep();
  testFeedGate();
  testWeather();
  testSettingsRoundtrip();
  testDemoFullRun();
  testAlertLedOwnership();
  testDemoBreathingRestore();

  testBoundaries();
  testIdempotency();
  testTimingBoundaries();

  printf("--- integration ---\n");
  testInoBoot();
  testHttpApi();
  testHttpLedOnOff();
  testHttpMechanisms();
  testHttpControlRoutes();
  testHttpRootFavicon();
  testOled();
  testSerialMenu();
  testSerialDemo();
  testWifiRecovery();

  printf("--- deep ---\n");
  testApiJsonContract();
  testResponseExactness();
  testOledLines();
  testSerialEdges();
  testDemoDiagnostics();
  testWifiThrottle();
  testBootOrder();
  testRebootPersistence();
  testFuzzSoak();
  testClockWrap();
  testBootDegraded();

  printf("=== %d checks, %d failures ===\n", gChecks, gFails);
  return gFails ? 1 : 0;
}
```

- Dòng 1978–1980: in banner với các cờ cấu hình lúc biên dịch — phiên bản core Arduino-ESP32 (`ESP_ARDUINO_VERSION_MAJOR`), hướng đảo tín hiệu cảm biến sáng/đất/nước (`INVERT_LIGHT/SOIL/WATER`), relay kích mức thấp và loại buzzer. Người đọc kết quả test nhìn banner là biết đang chạy cấu hình nào.
- Dòng 1982–1998: **nhóm unit test** — cảm biến trung bình (T1) → DHT failover → bơm hysteresis → an toàn nước thấp → quạt → đèn + PIR → breathing/LED → chạm → RGB → còi → cối xay gió → máng ăn → thời tiết → settings → demo đầy đủ (T15) → sở hữu LED của PIR (T16) → khôi phục breathing sau demo (T17). Chạy trước vì nhanh, cô lập được lỗi đơn vị.
- Dòng 2000–2002: **nhóm biên** — giá trị biên, tính idempotency (gọi lặp cho cùng kết quả), biên thời gian.
- Dòng 2004–2014: **nhóm integration** — boot qua file `.ino`, HTTP API, LED on/off, các cơ cấu, các route điều khiển, root/favicon, OLED, menu serial, serial demo, khôi phục WiFi. Đây là các test cho firmware đã "boot thật".
- Dòng 2016–2029: **nhóm deep** — hợp đồng JSON của API, độ chính xác phản hồi, các dòng OLED, biên serial, chẩn đoán demo, throttle WiFi (D10), thứ tự boot (D11), lưu bền settings qua chu kỳ nguồn (D12), fuzz soak (D13), tràn `millis()` (D14), boot suy giảm (I11).
- Dòng 2031: in tổng kết `=== %d checks, %d failures ===` với `gChecks` (tổng số kiểm tra đã chạy) và `gFails` (số kiểm tra thất bại).
- Dòng 2032: `return gFails ? 1 : 0` — trả mã thoát 1 nếu có lỗi để script/CI phát hiện, 0 nếu tất cả pass.

**Vì sao thứ tự gọi quan trọng?** Đây là chương trình C++ chạy một tiến trình duy nhất: biến `static`/toàn cục **không tự reset** giữa các test, và `resetState()` chỉ dọn một phần trạng thái. Các test sau dựa vào trạng thái do test trước để lại (ví dụ `setup()` nạp settings từ "flash" giả, các biến `static` trong `src/` như cờ cảnh báo PIR còn nguyên). Vì vậy bộ test được xếp từ **đơn vị → biên → integration → deep**, và các test có tác dụng phụ mạnh nhất (boot lại, fuzz, tràn đồng hồ, boot suy giảm) nằm cuối để không làm nhiễu những test khác. Khi một test đổi hành vi gọi test trước, kết quả có thể lệch theo — đây là lý do không nên đảo thứ tự tùy tiện trong `main()`.
---

## PHẦN 15 — test/mocks (giả lập phần cứng trên máy tính)

**mock** (giả lập) là các thư viện thay thế tạm cho phần cứng và thư viện ESP32 thật, nhưng giữ nguyên tên lớp/tên hàm mà firmware đang gọi. Nhờ vậy `test/run_tests.sh` chỉ cần biên dịch **SmartFarm.ino + src/*.cpp thật** bằng `g++` trên PC, với `test/mocks/` nằm trước trong đường dẫn include (`test/run_tests.sh:35`) — firmware không hề biết mình đang chạy trên máy tính. Test có thể tự đặt nhiệt độ/độ ẩm, điện áp ADC, trạng thái WiFi, tua đồng hồ `millis()`, rồi xem lại chính xác những gì firmware đã in ra OLED/Serial hoặc ghi xuống chân GPIO. Vì chạy trên code production nên mọi kiểm tra ở đây có giá trị như khi nạp lên kit.

### 15.1. mock.h — lời hứa (API) và các biến trạng thái (dòng 1–68)

`mock.h` là bản "hợp đồng" giữa bộ test và `mock.cpp`: nó liệt kê các biến test được phép đọc/ghi và các hàm test được phép gọi. Toàn bộ biến chia sẻ ở đây chỉ **khai báo** (`extern`), còn **định nghĩa thật** nằm trong `mock.cpp`.

**Khối 1 — Phần mở đầu (dòng 1–5)**

```cpp
#pragma once
// API mock phần cứng điều khiển được cho bộ test chạy trên host.
// Các test link firmware THẬT (SmartFarm.ino + src/*.cpp) với các
// mock này, nên mọi kiểm tra đều chạy trên code production. Xem test/README.md.
#include <cstdint>
```

- Dòng 1: `#pragma once` — chỉ cho phép mỗi file include `mock.h` một lần, tránh khai báo trùng.
- Dòng 2–4: comment nêu rõ triết lý: test link **firmware thật** với mock, nên kết quả test phản ánh đúng code production.
- Dòng 5: `#include <cstdint>` — cung cấp `uint8_t`, `uint32_t` cho các mảng bên dưới.
- Dòng 6: dòng trống phân nhóm (các dòng trống tương tự: 19, 31, 36, 42, 47, 57, 62).

**Khối 2 — Trạng thái phần cứng giả lập (dòng 7–18)**

```cpp
// ── Trạng thái phần cứng giả lập ──
extern float mockDhtTemp, mockDhtHum;   // giá trị đọc từ DHT11
extern int mockAdc[40];                 // giá trị analogRead theo chân
extern uint8_t mockDigital[40];         // giá trị digitalRead theo chân
extern uint32_t mockPinLevel[40];       // mức digitalWrite ghi lại theo chân
extern int mockServoAngle;              // góc servo.write() lần cuối
extern uint32_t mockPwm[40];            // giá trị ledcWrite() lần cuối theo kênh/chân
extern int mockBuzzerToneHz;            // tần số tone(), 0 = im lặng
extern uint32_t mockPixel[8];           // màu NeoPixel đã ghi
extern int mockWifiStatus;              // kết quả WiFi.status()
extern const char *mockWifiIp;          // chuỗi WiFi.localIP()
extern bool mockDisplayBeginOk;         // kết quả display.begin() (false = thiếu OLED)
```

- Dòng 8: `mockDhtTemp`, `mockDhtHum` — nhiệt độ/độ ẩm mà `DHT::readTemperature()`/`readHumidity()` sẽ trả về; test gán trước khi gọi `readSensors()`.
- Dòng 9: `mockAdc[40]` — bảng giá trị `analogRead()` theo chân 0..39; test gán ví dụ `mockAdc[LDR_PIN] = 3000` để giả lập trời sáng/tối.
- Dòng 10: `mockDigital[40]` — giá trị `digitalRead()` trả về (dùng cho cảm biến PIR, chạm...).
- Dòng 11: `mockPinLevel[40]` — mức mà firmware đã ghi ra chân bằng `digitalWrite()`; đây là căn cứ để test xác nhận bơm/quạt/relay bật hay tắt.
- Dòng 12: `mockServoAngle` — góc của lần `servo.write()` gần nhất, test đối chiếu cửa máng ăn/cối xay gió xoay đúng.
- Dòng 13: `mockPwm[40]` — giá trị `ledcWrite()` gần nhất, đánh theo **kênh** (core 2.x) hoặc theo **chân** (core 3.x) nên mảng 40 phần tử phủ được cả hai cách.
- Dòng 14: `mockBuzzerToneHz` — tần số `tone()`; bằng 0 nghĩa là đang im lặng (đã `noTone()`).
- Dòng 15: `mockPixel[8]` — màu (4 byte) của 8 bóng NeoPixel đã ghi qua `setPixelColor()`.
- Dòng 16: `mockWifiStatus` — mã mà `WiFi.status()` trả về; test đổi giữa `WL_CONNECTED` và `WL_DISCONNECTED` để thử mất mạng/nối lại.
- Dòng 17: `mockWifiIp` — chuỗi IP mà `WiFi.localIP()` hiển thị (cả trên OLED lẫn trang web).
- Dòng 18: `mockDisplayBeginOk` — kết quả `display.begin()`; đặt `false` để giả lập "không tìm thấy màn hình OLED" (test I11: boot suy giảm, mã lỗi 101/102).

**Vì sao `extern` ở `mock.h` mà phần định nghĩa lại nằm ở `mock.cpp`?**

- `extern` chỉ là **lời hứa**: "biến này có thật ở đâu đó, đừng cấp phát bộ nhớ ở đây". Nhờ vậy mọi file include `mock.h` (firmware lẫn test) đều nhìn thấy **cùng một** biến duy nhất.
- Nếu bỏ `extern`, viết thẳng `float mockDhtTemp;` trong header thì mỗi file `.cpp` include header sẽ tự tạo một bản sao riêng. Test gán giá trị vào bản sao của test, firmware lại đọc bản sao của firmware — test mất tác dụng.
- Vì thế `mock.cpp` (dòng 18–29) là nơi **định nghĩa thật** (không có `static`) cho tất cả biến này: đúng một nơi cấp phát, mọi nơi khác dùng chung.
- Ngược lại, các biến chỉ phục vụ nội bộ `mock.cpp` thì mới khai báo `static` (chỉ file đó thấy): `sMillis` (dòng 16), `sIoLog` (dòng 54), `sSerialOut`/`sSerialIn` (dòng 120–121), `sRoutes`/`sRouteCount` (dòng 151–152), `sArgs`/`sArgCount` (dòng 155–156), `sLastType`/`sLastBody`/`sLastCode` (dòng 168–169), `sOledText`/`sOledLines`/`sOledRow` (dòng 286–288), `sPrefs` (dòng 322). Khi test cần đọc/đổi chúng, `mock.cpp` mở hàm truy cập tương ứng: `mockMillis()` (dòng 49), `mockIoLog()` (dòng 63), `mockSerialOutput()` (dòng 145), `mockOledText()` (dòng 312), `mockServerLastBody()` (dòng 283)...
- Tóm lại: biến chia sẻ = `extern` ở header + định nghĩa (không `static`) ở `.cpp`; biến riêng tư = `static` ở `.cpp` + hàm truy cập trong cùng `.cpp`.

**Khối 3 — Bộ đếm instrumentation (dòng 20–30)**

```cpp
// ── Bộ đếm instrumentation (test chuyên sâu) ──
extern int mockDigitalWriteCount;       // số lần gọi digitalWrite()
extern int mockLedcWriteCount;          // số lần gọi ledcWrite()
extern int mockInvalidPixelWrites;      // setPixelColor với index ngoài phạm vi
extern int mockInvalidPwmWrites;        // ledcWrite với kênh/chân ngoài phạm vi
extern int mockServoWriteCount;         // số lần gọi servo.write()
extern int mockServoMinSeen;            // góc nhỏ nhất từng ghi (9999 nếu chưa có)
extern int mockServoMaxSeen;            // góc lớn nhất từng ghi (-1 nếu chưa có)
extern int mockToneCalls, mockNoToneCalls;
extern int mockWifiBeginCalls, mockWifiDisconnectCalls, mockWifiReconnectCalls;
extern int mockWdtInitCalls, mockWdtAddCalls, mockWdtResetCalls;
```

- "instrumentation" nghĩa là **đo lường/đếm** — bộ đếm giúp test kiểm tra những hành vi mà giá trị trạng thái không thể hiện hết: firmware có gọi thừa không, có ghi sai vùng nhớ không, servo có vượt giới hạn cơ khí không...
- Dòng 21–22: đếm số lần `digitalWrite()` / `ledcWrite()` — test tính bất biến (idempotency) đối chiếu: gọi `setFan(true)` hai lần chỉ được ghi chân một lần.
- Dòng 23–24: đếm các lần ghi **sai phạm vi** (index NeoPixel ngoài 0..7, kênh/chân PWM >= 40) — nếu khác 0 là có bug tràn mảng; lỗi này không làm sập chương trình nhưng test phát hiện được.
- Dòng 25–27: số lần `servo.write()`, góc nhỏ nhất/lớn nhất từng ghi; 9999 và -1 là hai giá trị "sentinel" (mốc đánh dấu) nghĩa là "chưa ghi lần nào", để test bất biến góc servo luôn nằm trong 0–180.
- Dòng 28: `mockToneCalls`, `mockNoToneCalls` — kiểm tra nhánh buzzer thụ động (`BUZZER_PASSIVE=1`) có gọi `tone()`/`noTone()` đúng không.
- Dòng 29: `mockWifiBeginCalls`, `mockWifiDisconnectCalls`, `mockWifiReconnectCalls` — kiểm tra logic chống spam WiFi (đúng 10.000 ms mới thử nối lại, test D-series).
- Dòng 30: `mockWdtInitCalls`, `mockWdtAddCalls`, `mockWdtResetCalls` — kiểm tra watchdog được cấu hình và được "cho ăn" (`reset`) đều đặn, kể cả khi boot suy giảm.

**Khối 4 — Hàm điều khiển thời gian (dòng 32–35)**

```cpp
unsigned long mockMillis();
void mockAdvance(unsigned long ms);
void mockSetMillis(unsigned long ms);
void mockReset();
```

- Dòng 32: `mockMillis()` — test đọc "đồng hồ giả" hiện tại (firmware thấy cùng giá trị qua `millis()`).
- Dòng 33: `mockAdvance(ms)` — tua đồng hồ tiến thêm `ms` mili-giây, mô phỏng thời gian trôi mà không phải chờ thật.
- Dòng 34: `mockSetMillis(ms)` — đặt đồng hồ về mốc tuyệt đối, dùng cho test tràn `millis()` (sau ~49,7 ngày) ở vòng D14.
- Dòng 35: `mockReset()` — đưa **toàn bộ** trạng thái mock (chân, ADC, WiFi, counters, Serial, OLED, prefs, server...) về mặc định; test gọi trước mỗi test case để các test độc lập nhau.

**Khối 5 — Serial (dòng 37–41)**

```cpp
// Serial
void mockSerialPush(const char *s);
const char *mockSerialOutput();
int mockSerialPending();
void mockSerialClear();
```

- Dòng 38: `mockSerialPush(s)` — bơm ký tự vào đầu thu (RX) như thể người dùng gõ qua Serial Monitor; firmware `Serial.read()` sẽ đọc được.
- Dòng 39: `mockSerialOutput()` — trả về toàn bộ những gì firmware đã in ra (menu, log, chẩn đoán) để test so khớp.
- Dòng 40: `mockSerialPending()` — số byte còn đang chờ trong hàng đợi RX.
- Dòng 41: `mockSerialClear()` — xóa sạch cả bộ đệm in ra lẫn bộ đệm nhận vào.

**Khối 6 — Log thứ tự gọi IO (dòng 43–46)**

```cpp
// Log thứ tự gọi IO cho test regression lỗi glitch lúc khởi động:
// mỗi mục có dạng "W:26=1;" (digitalWrite) và "M:26;" (pinMode).
const char *mockIoLog();
void mockIoLogClear();
```

- Dòng 43–44: comment giải thích mục đích — bắt **đúng thứ tự** các lệnh chân khi khởi động, phục vụ test hồi quy lỗi glitch (chốt mức đầu ra **trước** `pinMode`). Mỗi sự kiện là một mẩu `"W:<chân>=<mức>;"` hoặc `"M:<chân>;"`.
- Dòng 45: `mockIoLog()` — trả chuỗi log đã ghép, ví dụ `"W:26=0;M:26;W:26=0;..."`.
- Dòng 46: `mockIoLogClear()` — xóa log (thường gọi trong `mockReset()` khi bắt đầu test mới).

**Khối 7 — WebServer (dòng 48–57)**

```cpp
// WebServer
void mockServerReset();
int  mockServerRouteCount();
bool mockServerHasRoute(const char *path);
void mockServerSetArg(const char *name, const char *value);
bool mockServerInvoke(const char *path);
int  mockServerLastCode();
const char *mockServerLastType();
const char *mockServerLastBody();
```

- Dòng 49: `mockServerReset()` — quên hết route, tham số và response đã ghi (dùng trong `mockReset()`).
- Dòng 50: `mockServerRouteCount()` — đếm số route firmware đã đăng ký `server.on(...)`; test I1 yêu cầu đủ 14 route.
- Dòng 51: `mockServerHasRoute(path)` — kiểm tra một đường dẫn cụ thể đã được đăng ký chưa (ví dụ `/api`, `/on`, `/off`...).
- Dòng 52: `mockServerSetArg(name, value)` — "tiêm" tham số request giả, ví dụ `mockServerSetArg("device", "pump")`, để handler đọc `server.arg("device")` như request thật.
- Dòng 54: `mockServerInvoke(path)` — **gọi thẳng handler** của route, mô phỏng một request HTTP; trả `false` nếu đường dẫn chưa đăng ký.
- Dòng 55–57: đọc kết quả response mà handler đã gửi: mã HTTP (`200`, `400`, `101`, `102`...), kiểu nội dung (`text/html`, `application/json`...) và thân phản hồi.

**Khối 8 — OLED (dòng 59–62)**

```cpp
// OLED
const char *mockOledText();
const char *mockOledLine(int row);      // text ghi lại cho mỗi dòng text 8-px
int mockOledLineCount();
```

- Dòng 60: `mockOledText()` — toàn bộ chữ đã in ra màn hình dưới dạng một luồng liền mạch.
- Dòng 61: `mockOledLine(row)` — chữ của riêng dòng thứ `row` (mỗi dòng cao 8 pixel, hàng 0..7); dùng để test layout từng dòng và giới hạn 21 ký tự.
- Dòng 62: `mockOledLineCount()` — số dòng text của màn hình; mock luôn trả 8 (màn 128×64).

**Khối 9 — Hook hiện thực trong mock.cpp (dòng 64–68)**

```cpp
// Các hook hiện thực trong mock.cpp, được gọi từ các header mô phỏng
void mockServoWrite(int angle);
void mockNeoPixelSet(int index, uint32_t color);
float mockPrefGet(const char *key, float dflt);
void mockPrefPut(const char *key, float value);
```

- Đây là "cầu nối": các lớp trong header mô phỏng (`ESP32Servo.h`, `Adafruit_NeoPixel.h`, `Preferences.h`) viết hàm inline rất mỏng rồi gọi các hook này; thân thật nằm ở `mock.cpp`, nên test điều khiển/đọc lại được.
- Dòng 65: `mockServoWrite(angle)` — `Servo::write()` (header `ESP32Servo.h`) gọi vào đây.
- Dòng 66: `mockNeoPixelSet(index, color)` — `Adafruit_NeoPixel::setPixelColor()` gọi vào đây (có kiểm tra index 0..7).
- Dòng 67–68: `mockPrefGet`/`mockPrefPut` — `Preferences::getFloat/putFloat...` (header `Preferences.h`) quy đổi mọi kiểu về `float` rồi gọi vào đây, giả lập vùng nhớ flash/NVS.

### 15.2. mock.cpp — phần hiện thực (dòng 1–380)

Đây là "phần thân" của bản hợp đồng: mọi hàm/biến khai báo ở `mock.h` (và các hook của header mô phỏng) đều được hiện thực tại đây.

**Khối 1 — Include (dòng 1–14)**

```cpp
#include "mock.h"
#include "Arduino.h"
#include "DHT.h"
#include "ESP32Servo.h"
#include "Adafruit_NeoPixel.h"
#include "Adafruit_SSD1306.h"
#include "WebServer.h"
#include "Wire.h"
#include "WiFi.h"
#include "esp_task_wdt.h"

#include <cstdarg>
#include <string>
#include <vector>
```

- Dòng 1: include `mock.h` trước để nắm "lời hứa" API.
- Dòng 2–10: các header mô phỏng cùng thư mục; cần đủ để hiện thực/tham chiếu các lớp DHT, Servo, NeoPixel, SSD1306, WebServer, Wire, WiFi, watchdog.
- Dòng 12: `<cstdarg>` — cần `va_list`, `va_start`, `vsnprintf` cho `SerialStub::printf`.
- Dòng 13–14: `<string>` cho mọi bộ đệm (`std::string`); `<vector>` được include kèm nhưng file hiện không dùng trực tiếp.

**Khối 2 — Đồng hồ giả (dòng 16)**

```cpp
static unsigned long sMillis = 0;
```

- Dòng 16: đồng hồ duy nhất của thế giới giả, khởi đầu 0; `static` để chỉ `mock.cpp` được sửa trực tiếp, test phải đi qua `mockMillis()`/`mockAdvance()`/`mockSetMillis()`.

**Khối 3 — Định nghĩa trạng thái chia sẻ (dòng 18–29)**

```cpp
float mockDhtTemp = 25.0f;
float mockDhtHum = 60.0f;
int mockAdc[40] = {0};
uint8_t mockDigital[40] = {0};
uint32_t mockPinLevel[40] = {0};
int mockServoAngle = 0;
uint32_t mockPwm[40] = {0};
int mockBuzzerToneHz = 0;
uint32_t mockPixel[8] = {0};
int mockWifiStatus = WL_CONNECTED;
const char *mockWifiIp = "192.168.1.50";
bool mockDisplayBeginOk = true;
```

- Dòng 18–19: định nghĩa thật của `extern` ở `mock.h:8`; mặc định 25 °C / 60 % — hợp lý để test nào quên đặt vẫn chạy.
- Dòng 20–26: các mảng và biến trạng thái được khởi tạo 0; riêng `mockPixel[8]` đúng 8 bóng như phần cứng dự án dùng.
- Dòng 27: `WL_CONNECTED` (= 3, định nghĩa trong `mocks/WiFi.h:7`) — mặc định coi như đã có mạng.
- Dòng 28: IP mặc định `"192.168.1.50"` để OLED/trang chủ có nội dung in ra khi test không cần tự đặt.
- Dòng 29: mặc định `true` — màn hình "tồn tại"; test I11 sẽ đổi thành `false`.

**Khối 4 — Định nghĩa bộ đếm (dòng 31–40)**

```cpp
int mockDigitalWriteCount = 0;
int mockLedcWriteCount = 0;
int mockInvalidPixelWrites = 0;
int mockInvalidPwmWrites = 0;
int mockServoWriteCount = 0;
int mockServoMinSeen = 9999;
int mockServoMaxSeen = -1;
int mockToneCalls = 0, mockNoToneCalls = 0;
int mockWifiBeginCalls = 0, mockWifiDisconnectCalls = 0, mockWifiReconnectCalls = 0;
int mockWdtInitCalls = 0, mockWdtAddCalls = 0, mockWdtResetCalls = 0;
```

- Dòng 31–35: các counter "đếm từ 0".
- Dòng 36–37: hai mốc min/max servo khởi tạo ngược đời có chủ đích: `9999` (không góc nào nhỏ hơn nó) và `-1` (không góc nào lớn hơn nó); sau lần ghi đầu tiên chúng tự chỉnh về giá trị thật.
- Dòng 38–40: các counter còn lại, gộp nhiều biến trên một dòng theo nhóm chức năng.

**Khối 5 — Ba đối tượng toàn cục (dòng 42–44)**

```cpp
TwoWire Wire;
WiFiStub WiFi;
SerialStub Serial;
```

- Dòng 42–44: `Arduino.h:101`, `Arduino.h:106`, `WiFi.h:31` chỉ **khai báo** `extern` ba đối tượng này; đây là nơi **định nghĩa** chúng. Firmware gọi `Serial.println(...)`, `Wire.begin(...)`, `WiFi.status()` đều trỏ về đúng ba đối tượng duy nhất này.

**Khối 6 — Lõi Arduino: `millis`/`delay` giả (dòng 46–51)**

```cpp
// ── Lõi Arduino ──
unsigned long millis() { return sMillis; }
void delay(unsigned long ms) { sMillis += ms; }
unsigned long mockMillis() { return sMillis; }
void mockAdvance(unsigned long ms) { sMillis += ms; }
void mockSetMillis(unsigned long ms) { sMillis = ms; }
```

- Dòng 47: `millis()` — firmware gọi hàm này như thật, nhưng giá trị lấy từ đồng hồ giả `sMillis`.
- Dòng 48: `delay(ms)` — **không ngủ thật**, chỉ cộng thẳng `ms` vào đồng hồ rồi trả về ngay. Nhờ vậy các test "chờ" 10.000 ms hay 30.700 ms vẫn chạy xong trong tích tắc, mà mốc thời gian vẫn chính xác từng mili-giây (test D-series kiểm chống dội 300 ms, cửa ăn 2000 ms, nhịp thở 30 ms...).
- Dòng 49–51: ba hàm điều khiển dành riêng cho test; `mockAdvance` và `delay` cho kết quả giống nhau nhưng tách tên để phân biệt "firmware chờ" với "test tua".

**Khối 7 — Log thứ tự IO (dòng 53–64)**

```cpp
// ── Log thứ tự gọi IO ──
static std::string sIoLog;

static void ioLogAppend(char kind, uint8_t pin, int value) {
  char b[24];
  if (kind == 'W') snprintf(b, sizeof(b), "W:%u=%d;", (unsigned)pin, value);
  else             snprintf(b, sizeof(b), "M:%u;", (unsigned)pin);
  sIoLog += b;
}

const char *mockIoLog() { return sIoLog.c_str(); }
void mockIoLogClear() { sIoLog.clear(); }
```

- Dòng 54: `sIoLog` — chuỗi ghép dần tất cả sự kiện chân, `static` nên chỉ hàm trong file này thấy.
- Dòng 56–61: `ioLogAppend` — hàm phụ định dạng một mẩu log: loại `'W'` (write) thành `"W:<chân>=<mức>;"`, loại khác (thực tế là `'M'` — mode) thành `"M:<chân>;"`; dùng `snprintf` vào buffer 24 byte rồi nối vào `sIoLog`.
- Dòng 63: `mockIoLog()` — trả con trỏ tới chuỗi log để test so khớp; vì các sự kiện nối theo đúng thứ tự gọi nên bắt được lỗi glitch khởi động.
- Dòng 64: `mockIoLogClear()` — xóa log để bắt đầu giai đoạn theo dõi mới.

**Khối 8 — Chân vào/ra và ADC (dòng 66–73)**

```cpp
void pinMode(uint8_t pin, uint8_t) { ioLogAppend('M', pin, 0); }
void digitalWrite(uint8_t pin, uint8_t value) {
  if (pin < 40) mockPinLevel[pin] = value;
  mockDigitalWriteCount++;
  ioLogAppend('W', pin, value);
}
int digitalRead(uint8_t pin) { return pin < 40 ? mockDigital[pin] : 0; }
int analogRead(uint8_t pin) { return pin < 40 ? mockAdc[pin] : 0; }
```

- Dòng 66: `pinMode` — chỉ ghi log `'M'`; tham số thứ hai (INPUT/OUTPUT) không đặt tên vì mock không có phần cứng thật để đổi chiều chân.
- Dòng 67–71: `digitalWrite` — nếu chân hợp lệ (< 40) ghi mức vào `mockPinLevel` (test đọc lại được), luôn tăng `mockDigitalWriteCount`, và ghi log kèm mức. Chân >= 40 không làm tràn mảng.
- Dòng 72: `digitalRead` — trả đúng `mockDigital[pin]` mà test đã gán; chân ngoài phạm vi trả 0 thay vì đọc bừa.
- Dòng 73: `analogRead` — trả đúng `mockAdc[pin]`. **Chú ý: mock KHÔNG thêm jitter/nhiễu ngẫu nhiên** — giá trị đọc ra chính xác bằng thứ test gán, nhờ vậy test biên so sánh được đúng tại các mốc như 2999 vs 3000. Tính ngẫu nhiên (fuzz) do `test_main.cpp` tự thêm bằng `<random>`, không nằm trong mock.

**Khối 9 — PWM (LEDC) cho cả core 2.x lẫn 3.x (dòng 74–81)**

```cpp
void ledcSetup(uint8_t, double, uint8_t) {}
void ledcAttachPin(uint8_t, uint8_t) {}
void ledcWrite(uint8_t ch, uint32_t value) {
  mockLedcWriteCount++;
  if (ch >= 40) { mockInvalidPwmWrites++; return; }
  mockPwm[ch] = value;
}
void ledcAttach(uint8_t, uint32_t, uint8_t) {}
```

- Dòng 74–75: `ledcSetup`, `ledcAttachPin` — no-op (không làm gì), chỉ để đủ hàm cho đường core 2.x.
- Dòng 76–80: `ledcWrite` — tăng counter, chặn "kênh/chân" ngoài phạm vi (tăng `mockInvalidPwmWrites` rồi thoát, bắt bug tràn mảng), còn lại lưu giá trị vào `mockPwm[ch]` để test đọc độ sáng/stato LED xanh.
- Dòng 81: `ledcAttach` — no-op cho đường core 3.x.
- Vì sao đủ cả hai: `SmartFarm.ino:93–98` chọn `ledcAttach(BLUE_LED_PIN, ...)` khi core >= 3, ngược lại `ledcSetup(...) + ledcAttachPin(...)`; `src/actuators.cpp:62–68` tương tự với `ledcWrite` (core 3 truyền **chân**, core 2 truyền **kênh** `LED_PWM_CHANNEL = 15`). Mock phải cung cấp đủ cả hai bộ hàm để cả 4 bản build trong `--full` đều link được; test đọc `mockPwm[BLUE_LED_PIN]` hoặc `mockPwm[LED_PWM_CHANNEL]` tùy phiên bản (`test_main.cpp:83,85`). Mặc định `test/mocks/esp_arduino_version.h` đặt major = 3.

**Khối 10 — Buzzer: `tone`/`noTone` (dòng 82–89)**

```cpp
void tone(uint8_t, unsigned int frequency, unsigned long) {
  mockToneCalls++;
  mockBuzzerToneHz = (int)frequency;
}
void noTone(uint8_t) {
  mockNoToneCalls++;
  mockBuzzerToneHz = 0;
}
```

- Dòng 82–85: `tone` — đếm số lần gọi, lưu tần số vào `mockBuzzerToneHz`; chân và thời lượng bỏ qua. Chỉ dùng khi build `BUZZER_PASSIVE=1` (buzzer thụ động cần xung vuông — xem `src/actuators.cpp:24–27`).
- Dòng 86–89: `noTone` — đếm và trả `mockBuzzerToneHz` về 0 (im lặng).
- Với `BUZZER_PASSIVE=0` (mặc định), `actuators.cpp` gọi `digitalWrite(BUZZER_PIN, ...)` thay vì `tone`, nên kiểm qua `mockPinLevel`/`mockIoLog`; bản build `passive-lowrelay` kiểm tra nhánh thụ động qua hai hàm này.

**Khối 11 — WiFi (dòng 91–94)**

```cpp
// ── WiFi ──
void WiFiStub::begin(const char *, const char *) { mockWifiBeginCalls++; }
void WiFiStub::disconnect() { mockWifiDisconnectCalls++; }
void WiFiStub::reconnect() { mockWifiReconnectCalls++; }
```

- Dòng 92: `WiFi.begin()` chỉ đếm — không có kết nối thật nào xảy ra.
- Dòng 93: `WiFi.disconnect()` đếm số lần ngắt.
- Dòng 94: `WiFi.reconnect()` đếm số lần thử nối lại — căn cứ kiểm tra chống spam (10.000 ms/lần).
- Các hàm còn lại (`status()`, `localIP()`, `macAddress()`, `RSSI()`, `setSleep()`) được viết inline trong `mocks/WiFi.h`: `status()` đọc thẳng `mockWifiStatus`, `localIP()` đọc `mockWifiIp`. Muốn giả lập mất mạng, test chỉ cần gán `mockWifiStatus = WL_DISCONNECTED`.

**Khối 12 — Watchdog `esp_task_wdt` (dòng 96–101)**

```cpp
// ── esp_task_wdt ──
int esp_task_wdt_init(const esp_task_wdt_config_t *) { mockWdtInitCalls++; return 0; }
int esp_task_wdt_init(uint32_t, bool) { mockWdtInitCalls++; return 0; }
int esp_task_wdt_add(void *) { mockWdtAddCalls++; return 0; }
int esp_task_wdt_reset() { mockWdtResetCalls++; return 0; }
int esp_task_wdt_deinit() { return 0; }
```

- Dòng 97–98: hai **overload** (hàm cùng tên khác tham số) của `esp_task_wdt_init`: bản core 3 nhận con trỏ `esp_task_wdt_config_t` (struct định nghĩa ở `mocks/esp_task_wdt.h:4–8`), bản core cũ nhận `(timeout, panic)`. Cả hai đều đếm và trả 0 (thành công).
- Dòng 99: `esp_task_wdt_add` đếm — mô phỏng đăng ký task với watchdog.
- Dòng 100: `esp_task_wdt_reset` đếm — mỗi lần "cho chó ăn"; test I11 xác nhận watchdog vẫn được cho ăn ngay cả khi boot suy giảm (thiếu OLED + hết hạn WiFi).
- Dòng 101: `deinit` trả 0, không đếm vì firmware thường không dùng.

**Khối 13 — Cảm biến DHT (dòng 103–105)**

```cpp
// ── DHT ──
float DHT::readTemperature() { return mockDhtTemp; }
float DHT::readHumidity() { return mockDhtHum; }
```

- Dòng 104–105: hai hàm đọc DHT trả đúng giá trị test đã đặt (mặc định 25/60); mọi logic làm tròn, phát hiện giá trị lỗi NaN... đều nằm ở firmware thật `src/sensors.cpp`, không nằm ở mock.

**Khối 14 — Hook Servo / NeoPixel (dòng 107–117)**

```cpp
// ── Hook Servo / NeoPixel ──
void mockServoWrite(int angle) {
  mockServoAngle = angle;
  mockServoWriteCount++;
  if (angle < mockServoMinSeen) mockServoMinSeen = angle;
  if (angle > mockServoMaxSeen) mockServoMaxSeen = angle;
}
void mockNeoPixelSet(int index, uint32_t color) {
  if (index < 0 || index >= 8) { mockInvalidPixelWrites++; return; }
  mockPixel[index] = color;
}
```

- Dòng 108–113: `mockServoWrite` — được `Servo::write()` trong `mocks/ESP32Servo.h:9` gọi. Nó lưu góc, đếm, rồi tự cập nhật góc nhỏ nhất/lớn nhất từng thấy — test dùng để chứng minh servo không bao giờ vượt 0–180 (bất biến fuzz D-series).
- Dòng 114–117: `mockNeoPixelSet` — được `Adafruit_NeoPixel::setPixelColor()` trong `mocks/Adafruit_NeoPixel.h:14` gọi. Index ngoài 0..7 bị chặn và tăng `mockInvalidPixelWrites`; hợp lệ thì lưu màu thô (đúng công thức `(r<<16)|(g<<8)|b` mà header dùng ở dòng 15–17).

**Khối 15 — Ghi lại Serial (dòng 119–147)**

```cpp
// ── Ghi lại output Serial ──
static std::string sSerialOut;
static std::string sSerialIn;

void mockSerialWriteStr(const char *s) { if (s) sSerialOut += s; }
void mockSerialPrintLong(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); sSerialOut += b; }
void mockSerialPrintFloat(double v, int digits) { char b[40]; snprintf(b, sizeof(b), "%.*f", digits, v); sSerialOut += b; }
void mockSerialPrintOther() { sSerialOut += "<obj>"; }

int SerialStub::available() { return (int)sSerialIn.size(); }
int SerialStub::read() {
  if (sSerialIn.empty()) return -1;
  int c = (unsigned char)sSerialIn.front();
  sSerialIn.erase(0, 1);
  return c;
}
void SerialStub::printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  sSerialOut += buf;
}

void mockSerialPush(const char *s) { if (s) sSerialIn += s; }
const char *mockSerialOutput() { return sSerialOut.c_str(); }
int mockSerialPending() { return (int)sSerialIn.size(); }
void mockSerialClear() { sSerialOut.clear(); sSerialIn.clear(); }
```

- Dòng 120–121: hai bộ đệm `static`: `sSerialOut` (firmware in ra) và `sSerialIn` (test bơm vào như bàn phím).
- Dòng 123–126: bốn hàm mà `SerialStub` trong `mocks/Arduino.h:76–86` chuyển tiếp tới: chuỗi thì nối thẳng (có kiểm tra null), số dài in `%ld`, số thực in `%.*f` với số chữ số thập phân yêu cầu, kiểu không hỗ trợ thì ghi `"<obj>"` để test vẫn nhận biết đã có lệnh in.
- Dòng 128: `available()` — số byte đang chờ đọc.
- Dòng 129–134: `read()` — lấy ký tự **đầu hàng đợi**, xóa nó khỏi đệm, trả `-1` khi rỗng (đúng quy ước Arduino). Ép `(unsigned char)` để byte `0xFF` vẫn là 255 chứ không thành -1 — chi tiết được test D-series kiểm với "byte rác 0xFF".
- Dòng 135–142: `printf` — dùng `vsnprintf` đổ vào buffer 256 byte rồi nối vào `sSerialOut`; `va_list` cho phép nhận số tham số bất kỳ như printf thật.
- Dòng 144–147: API cho test: bơm chuỗi vào RX, đọc toàn bộ output, xem còn bao nhiêu byte chờ, và xóa cả hai bộ đệm.

**Khối 16 — Kho chứa route/arg/response của WebServer (dòng 149–169)**

```cpp
// ── mock WebServer ──
struct Route { std::string path; void (*fn)(); };
static Route sRoutes[24];
static int sRouteCount = 0;

struct Arg { std::string name; std::string value; };
static Arg sArgs[16];
static int sArgCount = 0;

static std::string sLastType, sLastBody;
static int sLastCode = 0;
```

- Dòng 150–152: `Route` gồm đường dẫn + **con trỏ hàm** handler (hàm không đối số, giống thư viện WebServer thật); mảng tĩnh 24 route là dư sức chứa 14 route firmware đăng ký.
- Dòng 154–156: `Arg` gồm tên/giá trị tham số; tối đa 16 tham số cho một request giả.
- Dòng 168–169: nơi lưu response gần nhất: kiểu nội dung, thân, mã HTTP (khởi tạo 0 = "chưa gửi gì").

**Khối 17 — `WebServer::on` (dòng 171–180)**

```cpp
void WebServer::on(const char *path, void (*handler)()) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == path) { sRoutes[i].fn = handler; return; }
  }
  if (sRouteCount < 24) {
    sRoutes[sRouteCount].path = path;
    sRoutes[sRouteCount].fn = handler;
    sRouteCount++;
  }
}
```

- Dòng 171–174: nếu đường dẫn đã có, chỉ **thay handler** rồi thoát — giữ hành vi idempotent (đăng ký lại không tạo route trùng, cần thiết vì nhiều test gọi lại phần setup).
- Dòng 175–179: chưa có thì thêm vào cuối mảng và tăng đếm; mảng đầy thì bỏ qua an toàn (không tràn).

**Khối 18 — `WebServer::send` (dòng 197–204)**

```cpp
void WebServer::send(int code, const char *type, const char *body) {
  sLastCode = code; sLastType = type ? type : ""; sLastBody = body ? body : "";
}
void WebServer::send(int code, const char *type, const String &body) {
  sLastCode = code; sLastType = type ? type : ""; sLastBody = body.std_str();
}
void WebServer::send(int code) { sLastCode = code; sLastType.clear(); sLastBody.clear(); }
void WebServer::send_P(int code, const char *type, const char *body) { send(code, type, body); }
```

- Dòng 197–199: bản nhận `const char*` — chép mã/kiểu/thân, thay con trỏ null bằng chuỗi rỗng.
- Dòng 200–202: bản nhận `const String&` — dùng `std_str()` (chuỗi `std::string` bên trong lớp `String` giả ở `mocks/Arduino.h:44`).
- Dòng 203: bản chỉ có mã — dùng cho các phản hồi không thân (ví dụ mã 101/102 ở test boot suy giảm).
- Dòng 204: `send_P` (bản PROGMEM của thư viện thật) chuyển tiếp nguyên xi sang `send`.

**Khối 19 — `arg` và `hasArg` (dòng 207–218)**

```cpp
String WebServer::arg(const char *name) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == name) return String(sArgs[i].value.c_str());
  }
  return String("");
}
bool WebServer::hasArg(const char *name) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == name) return true;
  }
  return false;
}
```

- Dòng 207–212: `arg` — tìm tham số theo tên, trả về `String` chứa giá trị; không thấy thì trả chuỗi rỗng (đúng hành vi WebServer thật, handler không cần kiểm null).
- Dòng 213–218: `hasArg` — chỉ cần biết có/không, trả `bool`; dùng để kiểm nhánh "thiếu tham số" (mã lỗi 400...).

**Khối 20 — Reset / đếm / kiểm route (dòng 235–248)**

```cpp
void mockServerReset() {
  sRouteCount = 0;
  sArgCount = 0;
  sLastCode = 0; sLastType.clear(); sLastBody.clear();
}
int mockServerRouteCount() { return sRouteCount; }
bool mockServerHasRoute(const char *path) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == (path ? path : "")) return true;
  }
  return false;
}
```

- Dòng 235–241: `mockServerReset` — quên sạch route, tham số, response; được gọi ở cuối `mockReset()`.
- Dòng 242: `mockServerRouteCount` — trả về số route; test I1 so với kỳ vọng 14.
- Dòng 243–248: `mockServerHasRoute` — duyệt tìm đường dẫn, xử lý `path` null bằng chuỗi rỗng cho an toàn.

**Khối 21 — Tiêm tham số request (dòng 249–258)**

```cpp
void mockServerSetArg(const char *name, const char *value) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == (name ? name : "")) { sArgs[i].value = value ? value : ""; return; }
  }
  if (sArgCount < 16) {
    sArgs[sArgCount].name = name ? name : "";
    sArgs[sArgCount].value = value ? value : "";
    sArgCount++;
  }
}
```

- Dòng 249–252: tên đã tồn tại thì **cập nhật giá trị** — test có thể đổi tham số giữa các lần `mockServerInvoke` mà không cần reset.
- Dòng 253–257: chưa có thì thêm mới nếu còn chỗ (tối đa 16); null được thay bằng chuỗi rỗng.

**Khối 22 — Gọi handler và đọc response (dòng 269–283)**

```cpp
bool mockServerInvoke(const char *path) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == (path ? path : "")) {
      sLastCode = 0; sLastType.clear(); sLastBody.clear();
      sRoutes[i].fn();
      sArgCount = 0;
      return true;
    }
  }
  return false;
}
int mockServerLastCode() { return sLastCode; }
const char *mockServerLastType() { return sLastType.c_str(); }
const char *mockServerLastBody() { return sLastBody.c_str(); }
```

- Dòng 269–273: tìm route khớp đường dẫn; không thấy trả `false` (test biết URL chưa được đăng ký).
- Dòng 272: xóa response cũ trước khi chạy — nếu handler quên gửi gì thì `mockServerLastCode()` là 0, test phát hiện ngay.
- Dòng 273: gọi `sRoutes[i].fn()` — đây chính là **handler thật của firmware** trong `src/webserver.cpp`, chạy với ADC/chân/thời gian giả.
- Dòng 274: `sArgCount = 0` — tham số chỉ sống trong một request, tránh rò rỉ sang request sau.
- Dòng 281–283: ba hàm đọc mã HTTP / kiểu nội dung / thân phản hồi mà handler vừa ghi.

**Khối 23 — Bộ đệm OLED (dòng 285–293)**

```cpp
// ── mock OLED: một luồng + theo từng dòng text (setCursor y/8) ──
static std::string sOledText;
static std::string sOledLines[8];
static int sOledRow = 0;

static void oledAppend(const std::string &s) {
  sOledText += s;
  sOledLines[sOledRow] += s;
}
```

- Dòng 286–288: ba biến `static`: cả màn hình như một luồng (`sOledText`), từng dòng riêng (`sOledLines[8]` ứng 8 dòng × 8 pixel = 64 pixel chiều cao), và dòng hiện tại (`sOledRow`).
- Dòng 290–293: `oledAppend` — mỗi ký tự in ra được ghi đồng thời vào cả hai nơi; test có thể kiểm tổng thể hoặc kiểm layout từng dòng.

**Khối 24 — `begin()` của OLED (dòng 295)**

```cpp
bool Adafruit_SSD1306::begin(int, int) { return mockDisplayBeginOk; }
```

- Dòng 295: trả về biến `mockDisplayBeginOk` (mặc định `true`). Test I11 đặt `false` trước khi gọi setup để giả lập **không tìm thấy màn hình** (lỗi I2C), kiểm tra nhánh boot suy giảm vẫn chạy an toàn, gửi mã lỗi 101/102 và cho watchdog ăn. Chi tiết thiết kế xem mục 15.3.

**Khối 25 — `clearDisplay` và `setCursor` (dòng 297–306)**

```cpp
void Adafruit_SSD1306::clearDisplay() {
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
}
void Adafruit_SSD1306::setCursor(int, int y) {
  sOledRow = y / 8;
  if (sOledRow < 0) sOledRow = 0;
  if (sOledRow > 7) sOledRow = 7;
}
```

- Dòng 297–301: `clearDisplay` — xóa cả luồng lẫn 8 dòng, đưa con trỏ về dòng 0 (giống màn hình xóa trắng).
- Dòng 302–306: `setCursor` — bỏ qua tọa độ x (mock chỉ cần biết dòng), lấy `y / 8` vì mỗi dòng text cao 8 pixel; kẹp chỉ số vào 0..7 để không tràn mảng. Cách này khớp với firmware `src/oled.cpp` dùng các mốc y = 0, 8, 16, 24... cho từng dòng.

**Khối 26 — Các overload `print` (dòng 307–311)**

```cpp
void Adafruit_SSD1306::print(const char *s) { oledAppend(s ? s : ""); }
void Adafruit_SSD1306::print(char c) { oledAppend(std::string(1, c)); }
void Adafruit_SSD1306::print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); oledAppend(b); }
void Adafruit_SSD1306::print(float v, int digits) { char b[40]; snprintf(b, sizeof(b), "%.*f", digits, v); oledAppend(b); }
void Adafruit_SSD1306::print(const IPAddress &ip) { oledAppend(ip.toString().c_str()); }
```

- Dòng 307: chuỗi — null được thay bằng rỗng (firmware hay dùng `F("...")`, mà `F(x)` là macro `x` ở `mocks/Arduino.h:16`, nên vẫn vào overload này).
- Dòng 308: một ký tự (`%` trong màn hình độ ẩm...).
- Dòng 309: số nguyên (`Light: 3000`, `P:` chân...).
- Dòng 310: số thực với số chữ số thập phân yêu cầu (`25.0`, `60`...).
- Dòng 311: địa chỉ IP — gọi `ip.toString()`, mà `IPAddress::toString()` đọc `mockWifiIp` (định nghĩa ở `mocks/WiFi.h:16–19`), nên IP trên OLED do test quyết định.

**Khối 27 — Hàm đọc nội dung OLED (dòng 312–314)**

```cpp
const char *mockOledText() { return sOledText.c_str(); }
const char *mockOledLine(int row) { return (row >= 0 && row < 8) ? sOledLines[row].c_str() : ""; }
int mockOledLineCount() { return 8; }
```

- Dòng 312: toàn bộ luồng ký tự — dùng cho so khớp thô hoặc đếm ký tự.
- Dòng 313: nội dung một dòng (0..7), dòng ngoài phạm vi trả chuỗi rỗng thay vì đọc bừa.
- Dòng 314: luôn trả 8 — cố định theo màn 128×64; test layout kiểm giới hạn 21 ký tự mỗi dòng.

**Khối 28 — Kho "flash" Preferences (dòng 316–322)**

```cpp
// ── mock Preferences ──
struct PrefEntry {
  char key[16];
  float value;
  bool used;
};
static PrefEntry sPrefs[24];
```

- Dòng 316–322: `Preferences` của ESP32 ghi vào NVS (phân vùng flash); mock thay bằng mảng tĩnh 24 phần tử trong RAM. Mỗi mục gồm khóa tối đa 15 ký tự + `'\0'`, giá trị `float`, cờ `used` (true = ô đã dùng). Chỉ mục nhỏ (settings) mới lưu, nên 24 ô là đủ. Vì là biến `static`, chỉ `mock.cpp` truy cập; test đi qua `mockPrefGet`/`mockPrefPut` hoặc lớp `Preferences` giả.

**Khối 29 — `mockPrefGet` (dòng 324–329)**

```cpp
float mockPrefGet(const char *key, float dflt) {
  for (int i = 0; i < 24; i++) {
    if (sPrefs[i].used && strcmp(sPrefs[i].key, key) == 0) return sPrefs[i].value;
  }
  return dflt;
}
```

- Dòng 324–329: tìm khóa bằng `strcmp`; tìm thấy trả giá trị đã lưu, không thấy trả giá trị mặc định `dflt` — đúng hành vi `Preferences::getFloat(key, default)` thật.

**Khối 30 — `mockPrefPut` (dòng 331–344)**

```cpp
void mockPrefPut(const char *key, float value) {
  for (int i = 0; i < 24; i++) {
    if (sPrefs[i].used && strcmp(sPrefs[i].key, key) == 0) { sPrefs[i].value = value; return; }
  }
  for (int i = 0; i < 24; i++) {
    if (!sPrefs[i].used) {
      strncpy(sPrefs[i].key, key, sizeof(sPrefs[i].key) - 1);
      sPrefs[i].key[sizeof(sPrefs[i].key) - 1] = '\0';
      sPrefs[i].value = value;
      sPrefs[i].used = true;
      return;
    }
  }
}
```

- Dòng 332–334: khóa đã tồn tại thì **ghi đè giá trị** (giống flash ghi lại cùng key).
- Dòng 335–343: chưa có thì tìm ô trống đầu tiên, sao chép khóa bằng `strncpy` (chừa 1 byte cho ký tự kết thúc), tự đặt `'\0'` ở byte cuối để chuỗi luôn hợp lệ dù khóa dài quá 15 ký tự; lưu giá trị, đánh dấu `used`.
- Hết 24 ô thì hàm im lặng bỏ qua — không crash. Lớp `Preferences` giả (`mocks/Preferences.h:12–16`) chuyển `putInt`/`putBool` thành `float` trước khi gọi vào đây, nên mock chỉ cần một kiểu lưu.
- Lưu ý: đây là "flash giả" trong RAM — `mockReset()` xóa sạch; nhờ vậy mỗi test bắt đầu với cấu hình trắng, còn test về bền vững cài đặt chỉ cần không gọi reset giữa chừng.

**Khối 31 — `mockReset()` (dòng 346–380)**

```cpp
void mockReset() {
  sMillis = 0;
  mockDhtTemp = 25.0f;
  mockDhtHum = 60.0f;
  memset(mockAdc, 0, sizeof(mockAdc));
  memset(mockDigital, 0, sizeof(mockDigital));
  memset(mockPinLevel, 0, sizeof(mockPinLevel));
  memset(mockPwm, 0, sizeof(mockPwm));
  memset(mockPixel, 0, sizeof(mockPixel));
  memset(sPrefs, 0, sizeof(sPrefs));
  mockServoAngle = 0;
  mockBuzzerToneHz = 0;
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "192.168.1.50";
  mockDisplayBeginOk = true;

  mockDigitalWriteCount = 0;
  mockLedcWriteCount = 0;
  mockInvalidPixelWrites = 0;
  mockInvalidPwmWrites = 0;
  mockServoWriteCount = 0;
  mockServoMinSeen = 9999;
  mockServoMaxSeen = -1;
  mockToneCalls = mockNoToneCalls = 0;
  mockWifiBeginCalls = mockWifiDisconnectCalls = mockWifiReconnectCalls = 0;
  mockWdtInitCalls = mockWdtAddCalls = mockWdtResetCalls = 0;

  sSerialOut.clear();
  sSerialIn.clear();
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
  sIoLog.clear();
  mockServerReset();
}
```

- Dòng 347: đưa đồng hồ giả về 0 — mỗi test bắt đầu ở "thời điểm 0".
- Dòng 348–349: DHT về mặc định 25 °C / 60 %.
- Dòng 350–355: xóa toàn bộ mảng trạng thái bằng `memset` (ADC, chân đọc, mức ghi, PWM, màu pixel, sPrefs) — nhanh và không sót phần tử.
- Dòng 356–360: servo, buzzer, WiFi, IP, màn hình OLED về mặc định.
- Dòng 362–371: tất cả counter về 0; hai sentinel servo được đặt lại (`9999`/`-1`) để phép so min/max bắt đầu sạch.
- Dòng 373–374: xóa bộ đệm Serial (cả in ra lẫn nhận vào).
- Dòng 375–377: xóa toàn bộ nội dung OLED (luồng + 8 dòng) và đưa con trỏ về dòng 0.
- Dòng 378: xóa log thứ tự IO.
- Dòng 379: gọi `mockServerReset()` để xóa route/arg/response.
- Ghi chú: `mock.cpp` không có hàm `setup()` riêng — giá trị mặc định được gán ngay khi khởi tạo biến toàn cục (dòng 18–40), còn `mockReset()` là "nút reset" để test chạy độc lập. Một số thứ **không** bị reset vì không nằm trong file này: các hằng cấu hình firmware, và bộ nhớ cấp phát động của firmware (firmware không dùng).

### 15.3. Adafruit_SSD1306.h — giả lập màn hình OLED (dòng 1–26)

Toàn bộ file:

```cpp
#pragma once
#include "Arduino.h"
#include "Wire.h"
#include "WiFi.h"

#define SSD1306_SWITCHCAPVCC 2
#define SSD1306_WHITE 1

// mock OLED: ghi lại mọi nội dung được in, cả dạng một luồng và theo từng dòng text
// (y của setCursor chọn dòng, y/8). Xem mock.cpp/mock.h.
class Adafruit_SSD1306 {
public:
  Adafruit_SSD1306(int, int, TwoWire *, int) {}
  bool begin(int, int);
  void clearDisplay();
  void display() {}
  void setTextColor(int) {}
  void setCursor(int x, int y);
  void setTextSize(int) {}
  void print(const char *s);
  void print(char c);
  void print(int v);
  void print(float v, int digits);
  void print(const IPAddress &ip);
  template <typename T> void print(T) { print("<obj>"); }
};
```

- Dòng 1: `#pragma once` — include một lần.
- Dòng 2: `Arduino.h` — cần `String`, kiểu số, macro... cho phần khai báo bên dưới.
- Dòng 3: `Wire.h` — vì constructor nhận con trỏ `TwoWire *`.
- Dòng 4: `WiFi.h` — vì có `print(const IPAddress &ip)` (lớp `IPAddress` định nghĩa ở đó).
- Dòng 6: `SSD1306_SWITCHCAPVCC` = 2 — hằng số thật của thư viện Adafruit, firmware truyền vào `display.begin(...)`; mock cần định nghĩa đủ để biên dịch, giá trị không quan trọng.
- Dòng 7: `SSD1306_WHITE` = 1 — tương tự cho `display.setTextColor(...)`; mock bỏ qua màu.
- Dòng 9–10: comment mô tả cách mock ghi nội dung và quy ước dòng text 8 pixel.
- Dòng 11: khai báo lớp `Adafruit_SSD1306` trùng tên lớp thật — nhờ vậy firmware `#include <Adafruit_SSD1306.h>` (firmware không hề sửa) sẽ lấy bản này khi `test/mocks/` đứng trước trong đường dẫn include.
- Dòng 12: `public:` — mọi thứ bên dưới đều gọi được từ firmware.
- Dòng 13: constructor 4 tham số `(int, int, TwoWire *, int)` đúng chữ ký thư viện thật (rộng, cao, bus I2C, chân reset), nhưng thân rỗng — không có màn hình thật để khởi tạo.
- Dòng 14: `bool begin(int, int);` — **chỉ khai báo, không viết thân trong header**. Đây là điểm mấu chốt: thân hàm nằm ở `mock.cpp:295` và trả về biến `mockDisplayBeginOk`. Nếu viết inline `{ return true; }` như các hàm khác thì kết quả luôn cố định, test không thể giả lập "không tìm thấy màn hình"; tách sang `mock.cpp` cho phép test đặt `mockDisplayBeginOk = false` để chạy nhánh suy giảm (I11: thiếu OLED, firmware phải gửi mã lỗi 101/102 mà vẫn chạy tiếp và cho watchdog ăn).
- Dòng 15: `clearDisplay()` — khai báo; thân ở `mock.cpp:297` vì phải xóa các bộ đệm trạng thái.
- Dòng 16: `display()` — inline rỗng; firmware gọi để "đẩy" nội dung ra màn hình nhưng mock ghi nhận ngay lúc `print`, nên không cần làm gì.
- Dòng 17: `setTextColor(int)` — inline rỗng; mock không mô phỏng màu.
- Dòng 18: `setCursor(int x, int y)` — khai báo; thân ở `mock.cpp:302` vì mock cần `y` để biết đang ghi dòng nào (dòng text = `y / 8`).
- Dòng 19: `setTextSize(int)` — inline rỗng; các test đều giả định cỡ chữ 1 (mỗi dòng 8 pixel, 21 ký tự).
- Dòng 20–24: năm overload `print` cho `const char*`, `char`, `int`, `float` (kèm số chữ số thập phân), `IPAddress` — đều **chỉ khai báo**, thân ở `mock.cpp:307–311` để ghi vào bộ đệm OLED. Nhờ có đủ overload mà `display.print(F("..."))`, `display.print(t, 1)`, `display.print(water)`... trong `src/oled.cpp` biên dịch y như với thư viện thật.
- Dòng 25: template `print(T)` — "lưới an toàn" cho kiểu nào không có overload riêng (ví dụ `unsigned`): gọi `print("<obj>")`, tức vẫn vào overload chuỗi và ghi dấu vết, không gây lỗi biên dịch. Đây là kỹ thuật **catch-all template** (bắt tất cả các kiểu còn lại).
- Dòng 26: đóng lớp.

Tóm lại, ba file này khớp nhau thành một cơ chế thống nhất: `mock.h` hứa API cho test, `mock.cpp` giữ toàn bộ trạng thái và hiện thực mọi hàm (kể cả `Adafruit_SSD1306::begin`), còn các header mô phỏng như `Adafruit_SSD1306.h` chỉ khai báo/thin-inline để firmware thật biên dịch được nguyên vẹn trên PC.
### 15.4. Arduino.h — giả lập thư viện Arduino lõi (dòng 1–106)

`test/mocks/Arduino.h` là bản thay thế (mock) cho `Arduino.h` thật. Khi build test trên PC, cờ `-I"$MOCKS"` trong `run_tests.sh` (dòng 35) khiến mọi `#include <Arduino.h>` trỏ vào file này thay vì toolchain ESP32, nhờ đó firmware thật chạy được ngay trên máy tính. File chỉ **khai báo** API và viết sẵn vài hàm inline; trạng thái (thời gian, chân, Serial...) nằm hết trong `test/mocks/mock.cpp`.

#### Khối 1 — Mở đầu và các include (dòng 1–9)

```cpp
#pragma once
// Mô phỏng phía host cho API lõi Arduino mà dự án này dùng.
// Build thẳng trên PC: không cần ESP32 toolchain. Xem test/README.md.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <math.h>
```

- Dòng 1: `#pragma once` — chỉ thị "chỉ include file này một lần", tránh khai báo trùng khi nhiều file cùng include.
- Dòng 2–3: chú thích tiếng Việt nói rõ đây là mock chạy trên host, không cần toolchain ESP32.
- Dòng 4: `<cstdint>` — cung cấp `uint8_t`, `uint32_t`... dùng trong các khai báo chân/pin bên dưới.
- Dòng 5: `<cstdio>` — cung cấp `snprintf` (dùng trong `String` và các mock).
- Dòng 6: `<cstring>` — cung cấp các hàm C như `strcmp`, `strncpy` mà các file firmware/mock dùng, giống việc `Arduino.h` thật kéo theo `string.h`.
- Dòng 7: `<cstdlib>` — cung cấp `atoi`, `atof` cho `String::toInt()`/`toFloat()`.
- Dòng 8: `<string>` — `std::string` là bộ nhớ nền của lớp `String` giả.
- Dòng 9: `<math.h>` — hàm toán học, đảm bảo firmware include `Arduino.h` là có sẵn như trên Arduino thật.

#### Khối 2 — Hằng số và macro (dòng 11–17)

```cpp
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define PROGMEM
#define F(x) x
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
```

- Dòng 11: `HIGH 1` — mức logic cao, giống Arduino thật; firmware so sánh `digitalRead(...) == HIGH` (ví dụ `src/sensors.cpp:38-39`).
- Dòng 12: `LOW 0` — mức logic thấp.
- Dòng 13: `INPUT 0` — chế độ chân vào; firmware dùng cho TOUCH/PIR (`SmartFarm.ino:89-90`).
- Dòng 14: `OUTPUT 1` — chế độ chân ra; mock `pinMode` bỏ qua giá trị này nên con số chỉ cần biên dịch được. File **không** định nghĩa `PULLUP`/`INPUT_PULLUP` vì firmware không dùng điện trở kéo nội.
- Dòng 15: `PROGMEM` — trên ESP32 macro này đánh dấu dữ liệu nằm trong flash; trên PC để trống, nên `static const char ROOT_HTML[] PROGMEM` (webserver.cpp:12) thành mảng thường.
- Dòng 16: `F(x) x` — trên Arduino `F()` giữ chuỗi trong flash; trên PC chỉ là chính chuỗi đó, nên `Serial.print(F("..."))` hoạt động y hệt.
- Dòng 17: `constrain(amt, low, high)` — macro kẹp giá trị trong khoảng, công thức giống hệt Arduino; firmware dùng để chặn ngưỡng nhập từ web (`src/webserver.cpp:437-449`).

#### Khối 3 — Lớp String giả (dòng 19–51)

```cpp
class String {
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); s_ = b; }
  String(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); s_ = b; }
  String(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); s_ = b; }
  String(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); s_ = b; }
  String(float v, int digits = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", digits, (double)v); s_ = b; }
  String(double v, int digits = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", digits, v); s_ = b; }
  void reserve(unsigned n) { s_.reserve(n); }
  int toInt() const { return atoi(s_.c_str()); }
  float toFloat() const { return (float)atof(s_.c_str()); }
  const char *c_str() const { return s_.c_str(); }
  unsigned length() const { return (unsigned)s_.size(); }
  char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  String &operator+=(const char *o) { s_ += (o ? o : ""); return *this; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  const std::string &std_str() const { return s_; }
private:
  std::string s_;
};
inline String operator+(const String &a, const char *b) { return String(a.std_str() + (b ? b : "")); }
inline String operator+(const char *a, const String &b) { return String(std::string(a ? a : "") + b.std_str()); }
inline String operator+(const String &a, const String &b) { return String(a.std_str() + b.std_str()); }
inline String operator+(const String &a, int b) { return String(a.std_str() + String(b).std_str()); }
```

- Dòng 19: `class String` — tên trùng lớp `String` thật để firmware không phải sửa gì. Điểm khác biệt: bản thật tự quản lý bộ nhớ heap, bản giả bọc `std::string`.
- Dòng 21: hàm tạo mặc định — chuỗi rỗng.
- Dòng 22: hàm tạo từ `const char*`, có kiểm tra `s ? s : ""` để con trỏ NULL không gây sập.
- Dòng 23: hàm tạo từ `std::string` — dùng nội bộ khi ghép chuỗi.
- Dòng 24–27: hàm tạo từ `int`/`long`/`unsigned int`/`unsigned long`, định dạng bằng `snprintf` với đúng ký hiệu `%d`/`%ld`/`%u`/`%lu` (buffer đủ lớn).
- Dòng 28–29: từ `float`/`double` với số chữ số thập phân mặc định 2 (`digits = 2`), in bằng `%.*f`; `float` phải ép sang `double` vì `snprintf` nhận tham số biến đổi.
- Dòng 30: `reserve(n)` — xin trước dung lượng, tương đương `String::reserve` thật (tránh cấp phát lại nhiều lần).
- Dòng 31: `toInt()` — đổi chuỗi thành số nguyên bằng `atoi`; web server dùng khi đọc tham số `r`, `g`, `b`, `angle`, `light` (`src/webserver.cpp:358-360,400,440`).
- Dòng 32: `toFloat()` — đổi chuỗi thành số thực bằng `atof`; dùng cho các ngưỡng (`src/webserver.cpp:437-442`).
- Dòng 33: `c_str()` — trả con trỏ ký tự bên trong để truyền cho hàm kiểu `printf`.
- Dòng 34: `length()` — số ký tự của chuỗi.
- Dòng 35: `charAt(i)` — lấy ký tự tại vị trí `i`, ngoài phạm vi trả về 0 (giống String thật).
- Dòng 36–37: `operator+=` — nối thêm `const char*` (an toàn NULL) hoặc `String` khác.
- Dòng 38–39: `operator==` — so sánh với `const char*` (an toàn NULL) hoặc `String`; nhờ đó `server.arg("on") == "true"` (webserver.cpp:339) chạy đúng.
- Dòng 44: `std_str()` — "cửa sau" lấy `std::string` bên trong; dùng khi ghép chuỗi và trong mock WebServer.
- Dòng 46: `std::string s_` — toàn bộ dữ liệu của String giả nằm ở đây (private).
- Dòng 48–51: các `operator+` tự do (không phải thành viên) cho mọi tổ hợp cần dùng: String+cstr, cstr+String, String+String, String+int. Nhờ vậy biểu thức cộng chuỗi viết như Arduino thật.

#### Khối 4 — Khai báo API phần cứng lõi (dòng 53–64)

```cpp
unsigned long millis();
void delay(unsigned long);
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
int analogRead(uint8_t);
void ledcSetup(uint8_t, double, uint8_t);
void ledcAttachPin(uint8_t, uint8_t);
void ledcWrite(uint8_t, uint32_t);
void ledcAttach(uint8_t, uint32_t, uint8_t);
void tone(uint8_t, unsigned int, unsigned long = 0);
void noTone(uint8_t);
```

Đây là **khai báo** (chỉ có tên hàm, dấu `;`); phần thân nằm trong `mock.cpp`:

- Dòng 53: `millis()` — trả "đồng hồ" ảo `sMillis` (mock.cpp:47). Test có thể đặt/đẩy thời gian bằng `mockSetMillis`/`mockAdvance`, nhờ đó mô phỏng nhiều giờ trong vài mili-giây thật.
- Dòng 54: `delay(ms)` — bản mock **không chờ** mà cộng thẳng `ms` vào `sMillis` (mock.cpp:48); logic đếm thời gian của firmware vẫn chạy đúng mà test không bị treo.
- Dòng 55: `pinMode` — mock ghi log `"M:pin;"` (mock.cpp:66) để test kiểm tra thứ tự "chốt mức ra trước khi pinMode" (chống glitch lúc khởi động).
- Dòng 56: `digitalWrite` — mock lưu mức vào `mockPinLevel[pin]`, tăng `mockDigitalWriteCount` và ghi log `"W:pin=value;"` (mock.cpp:67-71).
- Dòng 57: `digitalRead` — trả `mockDigital[pin]` (mock.cpp:72); test đặt trước để giả lập TOUCH/PIR.
- Dòng 58: `analogRead` — trả `mockAdc[pin]` (mock.cpp:73); test giả lập ánh sáng/độ ẩm đất/mực nước.
- Dòng 59–60: `ledcSetup`, `ledcAttachPin` — API PWM của core 2.x; mock để rỗng (mock.cpp:74-75).
- Dòng 61: `ledcWrite` — mock tăng `mockLedcWriteCount`, lưu giá trị vào `mockPwm[ch]`, kênh `>= 40` bị đếm vào `mockInvalidPwmWrites` (mock.cpp:76-80).
- Dòng 62: `ledcAttach` — API PWM mới của core 3.x (firmware gọi trong `SmartFarm.ino:94` khi `ESP_ARDUINO_VERSION_MAJOR >= 3`); mock để rỗng (mock.cpp:81).
- Dòng 63: `tone(pin, freq, duration = 0)` — tiếng còi; mock lưu tần số vào `mockBuzzerToneHz` và tăng `mockToneCalls` (mock.cpp:82-85).
- Dòng 64: `noTone` — tắt còi; mock tăng `mockNoToneCalls` và đưa tần số về 0 (mock.cpp:86-89).

#### Khối 5 — Hook ghi Serial (dòng 66–70)

```cpp
// Hook ghi lại output Serial (hiện thực trong mock.cpp)
void mockSerialWriteStr(const char *s);
void mockSerialPrintLong(long v);
void mockSerialPrintFloat(double v, int digits);
void mockSerialPrintOther();
```

- Dòng 66: chú thích nói rõ các hàm này được "cài" trong `mock.cpp`.
- Dòng 63–66: bốn hàm ghi nhận output Serial: chuỗi, số nguyên dài, số thực kèm số chữ số, và "kiểu khác". Chúng được **khai báo ở đây nhưng định nghĩa ở mock.cpp** (dòng 123–126) để chuỗi ghi nhận `sSerialOut` là biến private trong mock.cpp, và test đọc lại qua `mockSerialOutput()`.

#### Khối 6 — `SerialStub` (dòng 72–101)

```cpp
struct SerialStub {
  void begin(unsigned long) {}
  int available();
  int read();
  void print(char c) { char b[2] = {c, 0}; mockSerialWriteStr(b); }
  void print(const char *s) { mockSerialWriteStr(s); }
  void print(char *s) { mockSerialWriteStr(s); }
  void print(const String &s) { mockSerialWriteStr(s.c_str()); }
  void print(int v) { mockSerialPrintLong(v); }
  void print(unsigned int v) { mockSerialPrintLong((long)v); }
  void print(long v) { mockSerialPrintLong(v); }
  void print(unsigned long v) { mockSerialPrintLong((long)v); }
  void print(float v, int digits = 2) { mockSerialPrintFloat(v, digits); }
  void print(double v, int digits = 2) { mockSerialPrintFloat(v, digits); }
  template <typename T> void print(T) { mockSerialPrintOther(); }
  void println() { mockSerialWriteStr("\n"); }
  void println(char c) { print(c); mockSerialWriteStr("\n"); }
  void println(const char *s) { print(s); mockSerialWriteStr("\n"); }
  void println(char *s) { print(s); mockSerialWriteStr("\n"); }
  void println(const String &s) { print(s); mockSerialWriteStr("\n"); }
  void println(int v) { print(v); mockSerialWriteStr("\n"); }
  void println(unsigned int v) { print(v); mockSerialWriteStr("\n"); }
  void println(long v) { print(v); mockSerialWriteStr("\n"); }
  void println(unsigned long v) { print(v); mockSerialWriteStr("\n"); }
  void println(float v, int digits = 2) { print(v, digits); mockSerialWriteStr("\n"); }
  void println(double v, int digits = 2) { print(v, digits); mockSerialWriteStr("\n"); }
  template <typename T> void println(T) { mockSerialPrintOther(); mockSerialWriteStr("\n"); }
  void printf(const char *fmt, ...);
};
extern SerialStub Serial;
```

- Dòng 72: `struct SerialStub` — "giả lập" đối tượng `Serial`; tên khác `Serial` thật để tránh trùng, nhưng biến toàn cục bên dưới vẫn tên là `Serial`.
- Dòng 73: `begin(baud)` để rỗng — trên PC tốc độ truyền không có ý nghĩa.
- Dòng 74–75: `available()`/`read()` chỉ khai báo; `mock.cpp:128-134` hiện thực một hàng đợi nhập `sSerialIn` — test bơm ký tự bằng `mockSerialPush` để giả lập menu Serial (SmartFarm.ino:170-171).
- Dòng 76: `print(char)` — gói ký tự thành chuỗi 2 phần tử rồi ghi.
- Dòng 77: `print(const char*)` — ghi chuỗi trực tiếp.
- Dòng 78: `print(char*)` — overload **quan trọng**: biến `char stamp[40]` (logger.cpp:37) khi truyền vào hàm sẽ "phân rã" thành `char*`. Nếu chỉ có bản `const char*`, template `print(T)` phía dưới khớp chính xác `T = char*` (không cần thêm `const`) nên sẽ thắng và in ra `<obj>` (xem dòng 86 và `mockSerialPrintOther`), mất nội dung thật. Có overload riêng thì `char*` khớp thẳng và đi vào `mockSerialWriteStr`. `println(char*)` ở dòng 90 có lý do y hệt.
- Dòng 79: `print(const String&)` — chuyển qua `c_str()`.
- Dòng 80–85: các bản `int`, `unsigned int`, `long`, `unsigned long`, `float`, `double` — dùng `mockSerialPrintLong`/`mockSerialPrintFloat` để in đúng định dạng như Serial thật (số thực có mặc định 2 chữ số).
- Dòng 86: `template <typename T> print(T)` — "lưới hứng" cho mọi kiểu còn lại (ví dụ `IPAddress` nếu lọt vào đây): in đại diện `<obj>` và vẫn ghi nhận; nhờ nó firmware lạ vẫn biên dịch được. Chính vì có template này nên mọi overload cụ thể ở trên mới cần thiết.
- Dòng 87: `println()` — chỉ xuống dòng.
- Dòng 88–98: các `println` lần lượt gọi `print` tương ứng rồi thêm `"\n"`; bản `println(char*)` (dòng 90) bảo vệ trường hợp mảng `char[]` như đã nói.
- Dòng 99: `printf(fmt, ...)` — khai báo; `mock.cpp:135-142` dùng `vsnprintf` để định dạng rồi ghi vào `sSerialOut` (firmware gọi rất nhiều, ví dụ logger.cpp:59,65,117).
- Dòng 101: `extern SerialStub Serial` — biến toàn cục; định nghĩa thật ở `mock.cpp:44`, nên mọi file firmware đều dùng chung một đối tượng.

#### Khối 7 — `TwoWire`/`Wire` (dòng 103–106)

```cpp
struct TwoWire {
  void begin(int, int) {}
};
extern TwoWire Wire;
```

- Dòng 103–105: `TwoWire::begin(SDA, SCL)` để rỗng — firmware chỉ gọi `Wire.begin(OLED_SDA, OLED_SCL)` (SmartFarm.ino:109) để "mở bus I2C"; trên PC không có bus thật, nội dung OLED được giả lập ở tầng `Adafruit_SSD1306` trong mock.cpp.
- Dòng 106: `extern TwoWire Wire` — biến toàn cục, định nghĩa ở `mock.cpp:42`; có nó thì `#include <Wire.h>` (globals.h:6) biên dịch được trên PC (xem mục 15.5, file `Wire.h`).

### 15.5. Các header giả lập còn lại

Nhóm file dưới đây thay thế các thư viện ESP32/Arduino (NeoPixel, DHT, Servo, Preferences, WebServer, WiFi, Wire, watchdog, version core). Chúng dùng chung một mẫu thiết kế: **header chỉ khai báo lớp/hàm với API trùng thư viện thật**, còn **trạng thái và phần định nghĩa nằm trong `test/mocks/mock.cpp`** (và được lộ ra cho test qua `test/mocks/mock.h`). Làm vậy để: (1) nhiều file cùng include mà không bị định nghĩa trùng, (2) trạng thái giả lập chỉ có một bản duy nhất, (3) test có thể đọc/đặt trạng thái đó.

#### 15.5.1. `Adafruit_NeoPixel.h` (18 dòng)

```cpp
#pragma once
#include "Arduino.h"

#define NEO_GRB 0x52
#define NEO_KHZ800 0x0000

void mockNeoPixelSet(int index, uint32_t color);

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(int, int, int) {}
  void begin() {}
  void show() {}
  void setPixelColor(int index, uint32_t color) { mockNeoPixelSet(index, color); }
  uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  }
};
```

- Dòng 1–2: `#pragma once` và include `Arduino.h` (cần `uint32_t`).
- Dòng 4–5: hai hằng số copy từ thư viện thật (`NEO_GRB = 0x52`, `NEO_KHZ800 = 0x0000`) để dòng `Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800)` (globals.cpp:24) biên dịch được; giá trị chỉ là tham số, mock không dùng.
- Dòng 7: khai báo hook `mockNeoPixelSet` — định nghĩa ở `mock.cpp:114-117`.
- Dòng 11: hàm tạo nhận (số LED, chân, loại) nhưng bỏ qua — mock không cần chân thật.
- Dòng 12–13: `begin()`/`show()` rỗng — `show()` thật mới là lúc dữ liệu được "chốt" ra LED; mock lưu màu ngay khi `setPixelColor` nên test đọc được tức thì.
- Dòng 14: `setPixelColor` gọi hook; hook lưu màu vào `mockPixel[8]` (mock.cpp:26) và đếm các lần ghi sai chỉ số vào `mockInvalidPixelWrites` (mock.cpp:115).
- Dòng 15–17: `Color(r,g,b)` đóng gói 24 bit theo đúng công thức thư viện thật `0xRRGGBB`; nhờ vậy test so sánh được với các giá trị như `0xFF0000` (đỏ).

#### 15.5.2. `DHT.h` (13 dòng)

```cpp
#pragma once
#include "Arduino.h"

#define DHT11 11
#define DHT22 22

class DHT {
public:
  DHT(uint8_t, uint8_t) {}
  void begin() {}
  float readTemperature();
  float readHumidity();
};
```

- Dòng 1–2: `#pragma once` + include `Arduino.h`.
- Dòng 4–5: mã loại cảm biến `DHT11 = 11`, `DHT22 = 22` — trùng giá trị thư viện thật; firmware tạo `DHT dht(DHT_PIN, DHT_TYPE)` (globals.cpp:22).
- Dòng 9: hàm tạo (chân, loại) bỏ qua tham số.
- Dòng 10: `begin()` rỗng (thật ra chỉ khởi tạo chân).
- Dòng 11–12: `readTemperature()`/`readHumidity()` **chỉ khai báo**; định nghĩa ở `mock.cpp:104-105`, trả về hai biến `mockDhtTemp`/`mockDhtHum` (mock.cpp:18-19). Test đặt hai biến này để giả lập ngày nóng/lạnh, độ ẩm cao/thấp rồi kiểm tra logic tưới/quạt.

#### 15.5.3. `ESP32Servo.h` (10 dòng)

```cpp
#pragma once
#include "Arduino.h"

void mockServoWrite(int angle);

class Servo {
public:
  void attach(int) {}
  void write(int angle) { mockServoWrite(angle); }
};
```

- Dòng 1–2: `#pragma once` + include `Arduino.h`.
- Dòng 4: hook `mockServoWrite` — định nghĩa ở `mock.cpp:108-113`.
- Dòng 8: `attach(chân)` rỗng — trên PC không cần gắn chân PWM thật.
- Dòng 9: `write(góc)` gọi hook; hook lưu `mockServoAngle` (mock.cpp:23), đếm `mockServoWriteCount` và ghi nhận góc nhỏ nhất/lớn nhất (`mockServoMinSeen`/`mockServoMaxSeen`, mock.cpp:36-37). Test dùng để kiểm tra quét servo, cửa cho ăn, cối xay gió nằm trong 0–180°.

#### 15.5.4. `Preferences.h` (17 dòng)

```cpp
#pragma once
#include "Arduino.h"

float mockPrefGet(const char *key, float dflt);
void  mockPrefPut(const char *key, float value);

class Preferences {
public:
  bool begin(const char *, bool) { return true; }
  void end() {}
  void putFloat(const char *key, float value) { mockPrefPut(key, value); }
  void putInt(const char *key, int value) { mockPrefPut(key, (float)value); }
  void putBool(const char *key, bool value) { mockPrefPut(key, value ? 1.0f : 0.0f); }
  float getFloat(const char *key, float dflt) { return mockPrefGet(key, dflt); }
  int getInt(const char *key, int dflt) { return (int)mockPrefGet(key, (float)dflt); }
  bool getBool(const char *key, bool dflt) { return mockPrefGet(key, dflt ? 1.0f : 0.0f) != 0; }
};
```

- Dòng 1–2: `#pragma once` + include `Arduino.h`.
- Dòng 4–5: hai hook `mockPrefGet`/`mockPrefPut` — định nghĩa ở `mock.cpp:324-344`.
- Dòng 8: `begin(tên vùng, chỉ đọc)` luôn trả `true` — coi như bộ nhớ NVS luôn mở được; firmware khỏi phải xử lý lỗi.
- Dòng 9: `end()` rỗng.
- Dòng 10–12: `putFloat`/`putInt`/`putBool` đều quy về `mockPrefPut`: số nguyên ép sang `float`, `bool` thành `1.0f`/`0.0f`. Một kho `float` duy nhất cho mọi kiểu giúp đơn giản.
- Dòng 14–16: `getFloat`/`getInt`/`getBool` đọc qua `mockPrefGet`; `int` ép ngược lại, `bool` là "khác 0". Nếu khóa chưa có thì trả về giá trị mặc định `dflt` (mock.cpp:328).
- Trạng thái: bảng `sPrefs[24]` (khóa `char[16]` + giá trị) ở `mock.cpp:317-322`; `mockReset()` xóa sạch bảng (mock.cpp:355) để mô phỏng "mất điện về mặc định định mức" giữa các test.

#### 15.5.5. `WebServer.h` (18 dòng)

```cpp
#pragma once
#include "Arduino.h"

// mock WebServer: các route được ghi lại và test có thể gọi,
// tham số request được inject, response được ghi lại (xem mock.cpp/mock.h).
class WebServer {
public:
  WebServer(int) {}
  void on(const char *path, void (*handler)());
  void begin() {}
  void handleClient() {}
  void send(int code, const char *type, const char *body);
  void send(int code, const char *type, const String &body);
  void send(int code);
  void send_P(int code, const char *type, const char *body);
  String arg(const char *name);
  bool hasArg(const char *name);
};
```

- Dòng 1–2: `#pragma once` + include `Arduino.h`.
- Dòng 4–5: chú thích mô tả triết lý mock: route được ghi nhận, test gọi lại được, tham số được "tiêm" (inject) từ test, phản hồi được ghi lại.
- Dòng 8: hàm tạo nhận cổng nhưng bỏ qua.
- Dòng 9: `on(path, handler)` chỉ khai báo; `mock.cpp:171-180` lưu vào bảng `sRoutes[24]`, nếu trùng path thì cập nhật handler. Nhờ đó test I1 boot đếm đủ 14 route.
- Dòng 11–12: `begin()`/`handleClient()` rỗng — không có socket thật.
- Dòng 14–17: bốn biến thể `send` chỉ khai báo; `mock.cpp:197-204` ghi mã HTTP, content-type, body vào `sLastCode`/`sLastType`/`sLastBody`. Có hai bản body vì firmware dùng cả chuỗi `const char*` (trang HTML, webserver.cpp:271) lẫn `String` (JSON sinh động, webserver.cpp:301). `send_P` chuyển tiếp sang `send` thường (mock.cpp:204).
- Dòng 18–19: `arg`/`hasArg` chỉ khai báo; `mock.cpp:207-218` tra bảng tham số `sArgs[16]` do test "tiêm" bằng `mockServerSetArg` — `arg` trả chuỗi rỗng nếu không có, thay vì NULL.
- Trạng thái nằm ở `mock.cpp:150-169`; test gọi route bằng `mockServerInvoke` (mock.cpp:269-280), hàm này gọi handler rồi xóa tham số (giống một request kết thúc).

#### 15.5.6. `WiFi.h` (31 dòng)

```cpp
#pragma once
#include "Arduino.h"

#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_SCAN_COMPLETED 2
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED 6

extern int mockWifiStatus;
extern const char *mockWifiIp;
extern int mockWifiBeginCalls, mockWifiDisconnectCalls, mockWifiReconnectCalls;

class IPAddress {
public:
  String toString() const { return String(mockWifiIp); }
};

struct WiFiStub {
  int status() { return mockWifiStatus; }
  IPAddress localIP() { return IPAddress(); }
  String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
  int RSSI() { return -55; }
  void begin(const char *, const char *);
  void disconnect();
  void reconnect();
  void setSleep(bool) {}
};
extern WiFiStub WiFi;
```

- Dòng 1–2: `#pragma once` + include `Arduino.h`.
- Dòng 4–10: các mã trạng thái `WL_*` copy từ thư viện WiFi thật. Giá trị phải khớp vì firmware so sánh `WiFi.status() == WL_CONNECTED` (oled.cpp:59, wifi.cpp:13); test đặt `mockWifiStatus` bằng `WL_DISCONNECTED`... để giả lập mất mạng.
- Dòng 12: `mockWifiStatus` — trạng thái WiFi hiện tại (định nghĩa `mock.cpp:27`).
- Dòng 13: `mockWifiIp` — chuỗi địa chỉ IP (định nghĩa `mock.cpp:28`).
- Dòng 14: ba bộ đếm số lần gọi `begin`/`disconnect`/`reconnect` (định nghĩa `mock.cpp:39`), dùng cho test chống spam WiFi (đợi 10 giây).
- Dòng 16–19: lớp `IPAddress` tối giản với `toString()` dựng `String` từ `mockWifiIp` — đủ dùng cho `WiFi.localIP().toString()` (logger.cpp:117).
- Dòng 22: `status()` đọc biến trạng thái.
- Dòng 23: `localIP()` trả `IPAddress()` (nó đọc `mockWifiIp` khi cần).
- Dòng 24: `macAddress()` trả chuỗi cố định — chỉ để hiển thị/log.
- Dòng 25: `RSSI()` trả `-55` cố định — mức tín hiệu chỉ mang tính hiển thị.
- Dòng 26–28: `begin`/`disconnect`/`reconnect` chỉ khai báo; `mock.cpp:92-94` chỉ tăng bộ đếm, không làm gì khác.
- Dòng 29: `setSleep()` rỗng.
- Dòng 31: `extern WiFiStub WiFi` — định nghĩa `mock.cpp:43`; tên trùng biến `WiFi` thật nên firmware không cần sửa.

#### 15.5.7. `Wire.h` (2 dòng)

```cpp
#pragma once
#include "Arduino.h"
```

- Dòng 1: `#pragma once`.
- Dòng 2: include `Arduino.h` — vì lớp `TwoWire` và biến `Wire` đã được định nghĩa ngay trong `Arduino.h` (dòng 103–106). File này tồn tại chỉ để dòng `#include <Wire.h>` (globals.h:6) trỏ đúng vào mock. Không cần mô phỏng bus I2C thật vì nội dung OLED được giả lập ở tầng `Adafruit_SSD1306` trong mock.cpp.

#### 15.5.8. `esp_task_wdt.h` (14 dòng)

```cpp
#pragma once
#include <cstdint>

struct esp_task_wdt_config_t {
  uint32_t timeout_ms;
  uint32_t idle_core_mask;
  bool trigger_panic;
};

int esp_task_wdt_init(const esp_task_wdt_config_t *config);
int esp_task_wdt_init(uint32_t timeout_s, bool panic);
int esp_task_wdt_add(void *handle);
int esp_task_wdt_reset();
int esp_task_wdt_deinit();
```

- Dòng 1–2: `#pragma once` + `<cstdint>` cho `uint32_t`.
- Dòng 4–8: `esp_task_wdt_config_t` mô phỏng struct cấu hình watchdog của ESP-IDF gồm 3 trường: thời gian chờ (ms), mặt nạ core nhàn rỗi, cờ "panic khi hết hạn". Firmware (core 3.x) tạo biến này rồi gọi `esp_task_wdt_init(&wdtCfg)` (SmartFarm.ino:67-73).
- Dòng 10: overload nhận con trỏ cấu hình — nhánh core 3.x.
- Dòng 11: overload nhận `(timeout_s, panic)` — nhánh core 2.x (SmartFarm.ino:75). Có hai bản để firmware biên dịch với cả hai dòng core.
- Dòng 12–14: `add`/`reset`/`deinit` — thêm task, "cho ăn" watchdog, hủy. Tất cả trả `int` (đơn giản hóa `esp_err_t`).
- Cả năm hàm chỉ khai báo; `mock.cpp:97-101` định nghĩa chúng: luôn trả 0 (thành công), tăng `mockWdtInitCalls`/`mockWdtAddCalls`/`mockWdtResetCalls`. Watchdog giả không bao giờ "cắn", nhưng bộ đếm giúp test kiểm tra firmware có khởi tạo và định kỳ reset nó (ví dụ test I11 sau khi thiếu OLED).

#### 15.5.9. `esp_arduino_version.h` (4 dòng)

```cpp
#pragma once
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 3
#endif
```

- Dòng 1: `#pragma once`.
- Dòng 2–4: nếu biến `ESP_ARDUINO_VERSION_MAJOR` chưa được định nghĩa thì gán mặc định bằng 3. Trên ESP32 thật, header này do core cung cấp; trên PC, mock đóng vai trò đó. `run_tests.sh` vẫn truyền `-DESP_ARDUINO_VERSION_MAJOR=2`/`=3` khi build (dòng 50–54), và định nghĩa từ dòng lệnh thắng vì `#ifndef` kiểm tra trước. Có biến này thì các nhánh `#if ESP_ARDUINO_VERSION_MAJOR >= 3` (SmartFarm.ino:67,93; actuators.cpp:63) mới chọn đúng API (ledcAttach hay ledcSetup/ledcAttachPin).

---

## PHẦN 16 — test/run_tests.sh (script build & chạy)

`test/run_tests.sh` là điểm vào duy nhất để build và chạy bộ test trên máy tính: nó biên dịch firmware thật (`SmartFarm.ino` + `src/*.cpp`) cùng mock phần cứng, rồi chạy một trong các cấu hình. Chế độ `--quick` chạy 1 bản build, `--full` chạy 4 bản build (core 3 mặc định, core 2 đảo cảm biến, còi thụ động + relay mức thấp, ASan+UBSan), còn `--coverage` build với `gcov` để đo độ phủ theo dòng và chặn nếu dưới ngưỡng `COVERAGE_MIN` (mặc định 80%). Nhờ `set -e`, chỉ cần một bản build/test lỗi là script dừng ngay với mã thoát khác 0 — tiện cho cả người chạy tay lẫn CI.

### 16.1. run_tests.sh — giải thích từng dòng (dòng 1–164)

#### Khối 1 — Shebang và mô tả cách dùng (dòng 1–13)

```bash
#!/usr/bin/env bash
# Trình chạy test chuyên sâu phía host cho firmware Smart Farm QE002.
#
# Biên dịch firmware THẬT (SmartFarm.ino + src/*.cpp) dựa trên các
# mock phần cứng có thể điều khiển trong test/mocks/ và chạy bộ test.
#
# Cách dùng:
#   ./test/run_tests.sh --quick      một bản build (mặc định core 3.x, cấu hình kit mặc định)
#   ./test/run_tests.sh --full       bốn bản build: core3, core2+inverts,
#                                    buzzer thụ động + relay kích mức thấp, ASan+UBSan
#   ./test/run_tests.sh --coverage   build với gcov và in độ phủ theo dòng
#
# Yêu cầu: g++ với C++17, bash. Không cần toolchain ESP32.
```

- Dòng 1: shebang — bảo hệ điều hành chạy file này bằng `bash` tìm qua biến môi trường `PATH` (`env bash`).
- Dòng 2: chú thích mục đích: trình chạy test chuyên sâu phía host cho firmware Smart Farm QE002.
- Dòng 3–6: giải thích script biên dịch firmware **thật** dựa trên mock phần cứng điều khiển được trong `test/mocks/`.
- Dòng 7–11: liệt kê ba chế độ chạy: `--quick` (1 bản build, core 3.x mặc định), `--full` (4 bản build: core3, core2 kèm đảo cảm biến, còi thụ động + relay kích mức thấp, ASan+UBSan), `--coverage` (build với gcov, in độ phủ theo dòng).
- Dòng 12–13: yêu cầu môi trường: `g++` chuẩn C++17 và `bash`; không cần toolchain ESP32.

#### Khối 2 — `set` an toàn và các đường dẫn (dòng 14–20)

```bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$ROOT/test"
MOCKS="$TEST_DIR/mocks"
BUILD="$TEST_DIR/build"
mkdir -p "$BUILD"
```

- Dòng 14: `set -euo pipefail` — ba tùy chọn an toàn: `-e` dừng ngay khi một lệnh lỗi; `-u` báo lỗi khi dùng biến chưa khai báo; `-o pipefail` khiến cả đường ống (`|`) thất bại nếu bất kỳ lệnh nào trong đó thất bại. Nhờ vậy lỗi build/test không bị "chìm".
- Dòng 16: `ROOT` — thư mục gốc repo: lấy thư mục chứa script (`${BASH_SOURCE[0]}` là đường dẫn file này), đi lên một cấp (`/..`), rồi `pwd` để có đường dẫn tuyệt đối; `cd` chạy trong subshell nên không đổi thư mục hiện tại.
- Dòng 17: `TEST_DIR="$ROOT/test"`.
- Dòng 18: `MOCKS="$TEST_DIR/mocks"` — nơi chứa các header/nguồn giả lập.
- Dòng 19: `BUILD="$TEST_DIR/build"` — thư mục chứa file build tạm (đã nằm trong `.gitignore`).
- Dòng 20: `mkdir -p` tạo thư mục build nếu chưa có; `-p` không báo lỗi khi thư mục đã tồn tại.

#### Khối 3 — Danh sách nguồn, cờ biên dịch (dòng 22–36)

```bash
SRC=(
  "$ROOT/src/actuators.cpp"
  "$ROOT/src/config.cpp"
  "$ROOT/src/control.cpp"
  "$ROOT/src/demo.cpp"
  "$ROOT/src/globals.cpp"
  "$ROOT/src/logger.cpp"
  "$ROOT/src/oled.cpp"
  "$ROOT/src/sensors.cpp"
  "$ROOT/src/settings.cpp"
  "$ROOT/src/webserver.cpp"
  "$ROOT/src/wifi.cpp"
)
FLAGS=(-std=gnu++17 -Wall -Wextra -Wshadow -Wundef -I"$MOCKS" -I"$ROOT/src")
FILES=("$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}" -x c++ "$ROOT/SmartFarm.ino")
```

- Dòng 22–34: mảng `SRC` liệt kê đủ 11 file nguồn production trong `src/` — đây là code thật của firmware.
- Dòng 35: `FLAGS` gồm: `-std=gnu++17` (chuẩn C++17), `-Wall -Wextra` (bật cảnh báo), `-Wshadow` (cảnh báo biến che khuất), `-Wundef` (cảnh báo dùng macro chưa định nghĩa trong `#if`), `-I"$MOCKS"` **đặt trước** `-I"$ROOT/src"` để `#include <Arduino.h>` trỏ vào mock chứ không phải thư viện thật.
- Dòng 36: `FILES` là danh sách đầy đủ đưa cho `g++`: test chính, `mock.cpp`, toàn bộ `SRC`, rồi `-x c++` + `SmartFarm.ino`. Hai điểm đáng chú ý: (1) `-x c++` báo cho g++ "file tiếp theo là C++" vì đuôi `.ino` không được nhận diện mặc định; (2) thứ tự đặt sao cho `-x` chỉ ảnh hưởng đúng file `.ino` ở cuối.

#### Khối 4 — Chế độ chạy và ngưỡng phủ (dòng 38–39)

```bash
MODE="${1:---full}"
COVERAGE_MIN="${COVERAGE_MIN:-80}"
```

- Dòng 38: `MODE="${1:---full}"` — lấy tham số dòng lệnh thứ nhất; nếu người dùng không truyền gì thì mặc định là `--full` (chạy đủ 4 bản build).
- Dòng 39: `COVERAGE_MIN="${COVERAGE_MIN:-80}"` — ngưỡng độ phủ tối thiểu, mặc định 80%; có thể ghi đè bằng biến môi trường khi chạy, ví dụ `COVERAGE_MIN=90 ./test/run_tests.sh --coverage`.

#### Khối 5 — Hàm `build_and_run` (dòng 41–48)

```bash
build_and_run() {
  local name="$1"; shift
  local bin="$BUILD/$name.bin"
  echo "=== Build: $name ==="
  g++ "${FLAGS[@]}" "$@" -o "$bin" "${FILES[@]}"
  "$bin"
  echo
}
```

- Dòng 41: định nghĩa hàm `build_and_run`.
- Dòng 42: `local name="$1"` nhận tên bản build; `shift` bỏ tham số đầu để `$@` còn lại là các cờ `-D...` riêng cho cấu hình đó.
- Dòng 43: đường dẫn file thực thi, ví dụ `test/build/core3.bin`.
- Dòng 44: in dấu `=== Build: ... ===` để người đọc biết đang ở bản build nào.
- Dòng 45: gọi `g++` với cờ chung (`FLAGS`), cờ riêng cấu hình (`"$@"`), `-o` file ra, rồi danh sách `FILES`. Lưu ý mọi biến đều để trong dấu ngoặc kép + `[@]` để đường dẫn có khoảng trắng vẫn an toàn.
- Dòng 46: chạy file vừa build. Nếu test thất bại, `set -e` (dòng 14) dừng script ngay — không có dòng "PASSED" nào được in.
- Dòng 47: in một dòng trống ngăn cách giữa các bản build.

#### Khối 6 — Ba cấu hình thường (dòng 50–54)

```bash
run_default()  { build_and_run core3 -DESP_ARDUINO_VERSION_MAJOR=3; }
run_core2()    { build_and_run core2-invert -DESP_ARDUINO_VERSION_MAJOR=2 \
                   -DINVERT_LIGHT=1 -DINVERT_SOIL=1 -DINVERT_WATER=1; }
run_buzzer()   { build_and_run passive-lowrelay -DESP_ARDUINO_VERSION_MAJOR=3 \
                   -DBUZZER_PASSIVE=1 -DRELAY_ACTIVE_LOW=1; }
```

- Dòng 50: `run_default` — bản `core3`, mô phỏng kit mặc định chạy ESP32 Arduino core 3.x (định nghĩa `ESP_ARDUINO_VERSION_MAJOR=3`).
- Dòng 51–52: `run_core2` — bản `core2-invert`, chạy core 2.x và bật đồng thời `INVERT_LIGHT`, `INVERT_SOIL`, `INVERT_WATER` để kiểm tra nhánh đảo chiều cảm biến (tức điện áp tăng khi trời tối/đất khô...). Dấu `\` cuối dòng 51 là nối dòng lệnh.
- Dòng 53–54: `run_buzzer` — bản `passive-lowrelay`, còi thụ động dùng `tone()` (`BUZZER_PASSIVE=1`) và relay kích mức thấp (`RELAY_ACTIVE_LOW=1`); đây là hai biến thể phần cứng khác kit mặc định.

#### Khối 7 — Bản build ASan + UBSan (dòng 57–66)

```bash
run_asan() {
  local bin="$BUILD/asan.bin"
  echo "=== Build: asan+ubsan ==="
  g++ "${FLAGS[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DESP_ARDUINO_VERSION_MAJOR=3 -o "$bin" "${FILES[@]}"
  # Một số phiên desktop đặt biến LD_PRELOAD (ví dụ libgtk-nocsd); runtime
  # sanitizer phải là thư viện được nạp đầu tiên, nên bỏ LD_PRELOAD cho lần chạy này.
  env -u LD_PRELOAD "$bin"
  echo
}
```

- Dòng 57: định nghĩa hàm `run_asan`.
- Dòng 58–59: file ra `asan.bin` và dòng thông báo build.
- Dòng 60–61: biên dịch kèm `-g` (giữ thông tin gỡ lỗi để sanitizer in đúng số dòng), `-fsanitize=address,undefined` (bật AddressSanitizer phát hiện truy cập bộ nhớ sai và UndefinedBehaviorSanitizer phát hiện hành vi không xác định), `-fno-omit-frame-pointer` (giữ con trỏ khung để stack trace đầy đủ), cấu hình core 3.
- Dòng 62–63: chú thích giải thích vì sao chạy bằng `env -u LD_PRELOAD`: một số phiên desktop đặt `LD_PRELOAD` (ví dụ `libgtk-nocsd`) khiến thư viện khác được nạp trước runtime của ASan; ASan bắt buộc phải nạp đầu tiên, nên phải **bỏ biến này cho riêng lệnh chạy** (`-u` = unset), không ảnh hưởng phần còn lại của script.
- Dòng 64: chạy binary với `LD_PRELOAD` đã bỏ; nếu sanitizer phát hiện lỗi, mã thoát khác 0 và `set -e` dừng script.
- Dòng 65: dòng trống ngăn cách.

#### Khối 8 — Coverage: build từng file với gcov (dòng 68–88)

```bash
run_coverage() {
  local cov="$BUILD/cov"
  rm -rf "$cov"
  mkdir -p "$cov"
  echo "=== Build: coverage (gcov) ==="
  local objs=()
  local f o
  # gcov tạo file ghi chú bằng cách thay phần mở rộng của file object bằng
  # .gcno, nên đặt tên object như "control.o" (không phải "control.cpp.o").
  for f in "$TEST_DIR/test_main.cpp" "$MOCKS/mock.cpp" "${SRC[@]}"; do
    o="$cov/$(basename "${f%.cpp}").o"
    g++ "${FLAGS[@]}" --coverage -c "$f" -o "$o"
    objs+=("$o")
  done
  g++ "${FLAGS[@]}" --coverage -c -x c++ "$ROOT/SmartFarm.ino" -o "$cov/SmartFarm.o"
  objs+=("$cov/SmartFarm.o")
  g++ --coverage -o "$cov/test.bin" "${objs[@]}"
  "$cov/test.bin" | tail -n 1
  echo
```

- Dòng 68: định nghĩa hàm `run_coverage`.
- Dòng 69–71: xóa sạch rồi tạo lại thư mục `test/build/cov` — tránh dữ liệu phủ cũ lẫn vào kết quả mới.
- Dòng 72: thông báo đang build chế độ coverage.
- Dòng 73–74: mảng `objs` chứa các file object và hai biến vòng lặp `f`, `o`.
- Dòng 77–78: chú thích then chốt: `gcov` tìm file ghi chú `.gcno` bằng cách thay đuôi của file object, nên object phải tên `control.o` — nếu đặt `control.cpp.o` thì gcov (được gọi trên `control.cpp`) sẽ tìm `control.gcno` và không thấy. Vì vậy `o="$cov/$(basename "${f%.cpp}").o"` cắt đuôi `.cpp` trước khi thêm `.o`.
- Dòng 79–83: vòng lặp biên dịch từng file test, mock và 11 file `src/` thành object riêng với cờ `--coverage` (tương đương `-fprofile-arcs -ftest-coverage`, sinh `.gcno` lúc biên dịch và `.gcda` lúc chạy), rồi thêm đường dẫn object vào mảng `objs`.
- Dòng 84–85: biên dịch `SmartFarm.ino` với `-x c++` thành `SmartFarm.o` và thêm vào `objs`.
- Dòng 86: liên kết tất cả object thành `test.bin`; bước link cũng cần `--coverage` để nối runtime gcov.
- Dòng 87: chạy test, nhưng chỉ giữ **dòng cuối** nhờ `tail -n 1` — vì binary in rất nhiều dòng kết quả, dòng cuối là dòng tổng kết. `pipefail` (dòng 14) đảm bảo nếu test thất bại thì cả đường ống thất bại.
- Dòng 88: dòng trống ngăn cách.

#### Khối 9 — Sinh file `.gcov` (dòng 90–94)

```bash
  echo "=== Coverage (production code: src/*.cpp + SmartFarm.ino) ==="
  ( cd "$cov"
    for f in "$ROOT/src/"*.cpp "$ROOT/SmartFarm.ino"; do
      gcov -o "$cov" "$f" > /dev/null 2>&1 || true
    done )
```

- Dòng 90: thông báo phạm vi đo: chỉ code production (`src/*.cpp` + `SmartFarm.ino`), không tính `test_main.cpp`/`mock.cpp`.
- Dòng 91: mở subshell (`(...)`) rồi `cd` vào thư mục `cov` — gcov ghi file `.gcov` vào thư mục hiện hành, và subshell giúp không đổi thư mục của script chính.
- Dòng 92–94: với từng file nguồn production, gọi `gcov -o "$cov"` (chỉ cho gcov biết nơi chứa `.gcno`/`.gcda`) và bỏ toàn bộ output thường lẫn output lỗi (`> /dev/null 2>&1`). `|| true` để một lần gcov "càu nhàu" không làm `set -e` dừng script; kết quả được đọc ở bước sau qua các file `.gcov`.

#### Khối 10 — Đọc từng file `.gcov` và tính độ phủ (dòng 96–128)

```bash
  local total=0 covered=0
  printf "%-30s %8s %8s %8s\n" "File" "Lines" "Hit" "Cover"
  local gc base stats hit tot pct
  for gc in "$cov"/*.gcov; do
    [ -e "$gc" ] || continue
    base="$(basename "$gc" .gcov)"
    case "$base" in
      actuators.cpp|config.cpp|control.cpp|demo.cpp|globals.cpp|logger.cpp|\
      oled.cpp|sensors.cpp|settings.cpp|webserver.cpp|wifi.cpp|SmartFarm.ino) ;;
      *) continue ;;
    esac
    stats="$(awk -F: '
      $2 ~ /^[ ]*[0-9]+$/ {
        line=$2 + 0;
        if (line == 0) next;
        c=$1; gsub(/ /,"",c);
        if (c == "-") next;              # không thực thi (chú thích, dòng trống, })
        total++;
        if (c != "#####" && c != "=====" && (c+0) > 0) covered++;
      }
      END { printf "%d %d", covered, total }
    ' "$gc")"
    hit="${stats%% *}"
    tot="${stats##* }"
    total=$((total + tot))
    covered=$((covered + hit))
    if [ "$tot" -gt 0 ]; then
      pct="$(awk -v c="$hit" -v t="$tot" 'BEGIN { printf "%.1f", 100.0*c/t }')"
    else
      pct="n/a"
    fi
    printf "%-30s %8d %8d %7s%%\n" "$base" "$tot" "$hit" "$pct"
  done
```

- Dòng 96: khởi tạo hai biến tổng `total` (dòng thực thi được) và `covered` (dòng đã chạy).
- Dòng 97: in tiêu đề bảng canh cột bằng `printf`: `File`, `Lines`, `Hit`, `Cover`.
- Dòng 98: khai báo các biến cục bộ dùng trong vòng lặp.
- Dòng 99: duyệt mọi file `.gcov` mà gcov vừa sinh.
- Dòng 100: nếu mẫu không khớp file nào (không có `.gcov`), `continue` để bỏ qua — vì bash có thể giữ nguyên chuỗi `"$cov"/*.gcov` khi không khớp.
- Dòng 101: lấy tên file bỏ đuôi `.gcov`, ví dụ `control.cpp`.
- Dòng 102–106: bộ lọc `case` — chỉ xử lý 12 tên thuộc code production (`actuators.cpp` ... `wifi.cpp`, `SmartFarm.ino`); mọi file khác (như `test_main.cpp.gcov`, `mock.cpp.gcov`) bị `continue` bỏ qua, đảm bảo độ phủ phản ánh đúng firmware.
- Dòng 107–117: đoạn `awk -F:` phân tích file `.gcov`. Mỗi dòng gcov có dạng `số_lần_chạy:số_dòng:nội_dung`, nên `$1` là số lần chạy, `$2` là số dòng.
  - Dòng 108: chỉ xử lý dòng mà cột 2 là số thuần (dòng nguồn thật); các dòng tiêu đề kiểu `function ...` bị loại.
  - Dòng 109–110: bỏ dòng số 0 (dòng tổng kết của gcov).
  - Dòng 111: lấy cột đếm và xóa khoảng trắng đệm.
  - Dòng 112: nếu cột đếm là `-` thì bỏ qua — đây là các dòng **không thực thi được** (chú thích, dòng trống, dấu `}`...). Vì sao phải bỏ: nếu tính cả chúng, mẫu số độ phủ bị thổi phồng và chỉ số không còn phản ánh số dòng thực sự chạy.
  - Dòng 113: `total++` — đếm một dòng thực thi được.
  - Dòng 114: `covered++` khi dòng không phải `#####` (chưa chạy), không phải `=====` (đường đặc biệt) và số lần chạy lớn hơn 0.
  - Dòng 116: `END` in ra hai số `covered total` cách nhau bằng dấu cách.
- Dòng 118–121: cắt chuỗi `stats` bằng tham số mở rộng của bash (`%% *` lấy từ cuối, `##* ` lấy từ đầu) rồi cộng dồn vào tổng toàn cục.
- Dòng 122–126: nếu file có ít nhất một dòng thực thi được thì tính phần trăm một chữ số thập phân bằng awk; nếu không thì ghi `n/a` để tránh chia cho 0.
- Dòng 127: in một hàng của bảng: tên file, tổng dòng, số dòng đã chạy, phần trăm.

#### Khối 11 — Dòng TOTAL và kiểm tra ngưỡng (dòng 130–141)

```bash
  local overall
  if [ "$total" -gt 0 ]; then
    overall="$(awk -v c="$covered" -v t="$total" 'BEGIN { printf "%.1f", 100.0*c/t }')"
  else
    overall="0.0"
  fi
  echo "--------------------------------------------------------"
  printf "%-30s %8d %8d %7s%%\n" "TOTAL" "$total" "$covered" "$overall"
  awk -v p="$overall" -v m="$COVERAGE_MIN" 'BEGIN { exit (p+0 >= m+0) ? 0 : 1 }' || {
    echo "ERROR: coverage ${overall}% is below the ${COVERAGE_MIN}% threshold (set COVERAGE_MIN to override)"
    exit 1
  }
```

- Dòng 130–135: tính `overall` — phần trăm phủ toàn dự án; nếu tổng số dòng bằng 0 thì gán `"0.0"` cho an toàn.
- Dòng 136: in dải phân cách.
- Dòng 137: in hàng `TOTAL` với tổng dòng, tổng dòng đã chạy và phần trăm chung.
- Dòng 138: `awk` so sánh số học `overall >= COVERAGE_MIN`; nếu đạt thì thoát 0, không đạt thì thoát 1. `p+0`/`m+0` ép chuỗi thành số. Nếu awk thoát 1, khối sau dấu `||` chạy.
- Dòng 139–140: in thông báo lỗi kèm gợi ý đặt `COVERAGE_MIN` để hạ/đổi ngưỡng, rồi `exit 1` — test coi như thất bại.

#### Khối 12 — Bộ chọn chế độ chạy (dòng 144–164)

```bash
case "$MODE" in
  --quick)
    run_default
    echo "QUICK: PASSED"
    ;;
  --full)
    run_default
    run_core2
    run_buzzer
    run_asan
    echo "FULL: ALL 4 BUILDS PASSED"
    ;;
  --coverage)
    run_coverage
    ;;
  *)
    echo "Usage: $0 [--quick|--full|--coverage]" >&2
    exit 2
    ;;
esac
```

- Dòng 144: `case "$MODE"` — rẽ nhánh theo chế độ đã lấy ở dòng 38.
- Dòng 145–147: `--quick` — chỉ chạy bản core3; nếu xong không lỗi thì in `QUICK: PASSED`. Vì có `set -e`, dòng này chỉ tới được khi test đã qua.
- Dòng 149–155: `--full` — lần lượt chạy đủ 4 bản build (core3, core2-invert, passive-lowrelay, asan+ubsan) rồi in `FULL: ALL 4 BUILDS PASSED`. Bất kỳ bản nào lỗi sẽ dừng ngay trước khi in dòng này.
- Dòng 157–158: `--coverage` — gọi `run_coverage`.
- Dòng 160–162: nhánh còn lại (`*`) — in hướng dẫn dùng ra **stderr** (`>&2`) rồi thoát mã 2 (mã quy ước "dùng sai tham số").
- Dòng 164: `esac` kết thúc `case`.

---

## PHẦN 17 — Bảng mã lỗi và thuật ngữ

### 17.1. Bảng mã lỗi (in ra dạng `[MA LOI: xxx]`)

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

### 17.2. Bảng chân — bản đầy đủ kèm tên biến trong code

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

### 17.3. Câu hỏi thường gặp khi đọc code

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
