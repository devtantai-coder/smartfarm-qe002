# Kiến trúc module Smart Farm QE002

## Cấu trúc thư mục

```
src/
├── *.h                 # header công khai ở gốc src/ (config, globals, logger, ...)
├── core/               # config.cpp, globals.cpp, logger.cpp, settings.cpp
├── hal/                # hal.cpp
├── sensors/            # sensors.cpp
├── actuators/          # actuators.cpp
├── control/            # control.cpp, demo.cpp, pid.cpp, softstart.cpp, calibration.cpp, anomaly.cpp
├── connectivity/       # wifi.cpp, webserver.cpp, mqtt.cpp, ota.cpp, history.cpp,
│                       # weather.cpp, notify.cpp, networker.cpp
├── ui/                 # oled.cpp
└── observability/      # metrics.cpp, health.cpp
```

Các file `.h` nằm ở `src/` gốc; file `.cpp` tương ứng nằm trong thư mục module.
Mỗi `.cpp` include header bằng đường dẫn tương đối `../<module>.h`.

## Runtime: loopTask chính + một network worker

Firmware chạy **single-loop superloop cho logic điều khiển**: web, cảm biến và cơ cấu
chấp hành dùng chung `SystemState` trên loopTask, không có task điều khiển song song.

Ngoại lệ duy nhất là **tác vụ mạng nền** (`src/connectivity/networker.cpp`): khi
`netWorkerInit()` chạy, nó tạo một FreeRTOS queue (`NETWORK_QUEUE_LEN`, mặc định 4 job)
và một task (`NETWORK_TASK_STACK`/`NETWORK_TASK_PRIORITY`). loopTask chỉ xếp job
Telegram/thời tiết vào queue (không chặn); worker thực hiện TLS/HTTP và trả kết quả
qua các biến có `IrqLock` (`src/irqlock.h`). Nhờ vậy một phiên TLS chậm không đóng
băng `loop()` và các interlock an toàn.

1. `setup()` khởi tạo watchdog, GPIO, cảm biến, OLED, NVS, WiFi, OTA (chỉ khi có
   mật khẩu), web server, `notifyInit()`, `weatherInit()`, `historyInit()` và
   `netWorkerInit()`.
2. `loop()` gọi lần lượt:
   - `server.handleClient()` — phục vụ HTTP;
   - `manageWiFi()` — kết nối lại WiFi không chặn;
   - `mqttLoop()`, `otaLoop()` — chỉ hoạt động khi module tương ứng được bật;
   - `ArduinoOTA.handle()` mỗi `OTA_HANDLE_INTERVAL`;
   - xử lý Serial/Bluetooth, lệnh debug và hẹn giờ khởi động lại;
   - `runAutomation()`;
   - `historyLoop()` (mẫu mỗi 30 giây) và `weatherLoop()` (xếp job Open-Meteo mỗi 15 phút,
     không tải trực tiếp trong loop);
   - vẽ OLED khi `oledDirty` hoặc đến `OLED_INTERVAL`;
   - deep sleep nếu `powerSave` được bật.

`runAutomation()` là nguồn logic duy nhất cho cả firmware và test host:
- đọc touch/PIR mỗi vòng;
- đọc DHT11/ADC mỗi `SENSOR_INTERVAL` (2 giây, bằng `MIN_INTERVAL` của thư viện DHT),
  lọc trung bình động, áp calibration;
- cập nhật anomaly score trên mẫu vừa lọc;
- kiểm tra an toàn bơm khô (kể cả hủy soft-start đang chờ) — chạy cả trong chế độ debug;
- nếu app ACEBOTT vừa gọi (`appControlActive()`), chỉ chạy `controlAlerts()`;
- chạy control/demo hoặc cập nhật demo; bơm tự động đi qua `softStartPump()`.

Nhờ single-loop, web handler, cảm biến và actuator dùng chung `SystemState` mà
không cần khóa; production và bộ test chạy đúng cùng một đường logic. Chỉ dữ liệu
do worker mạng ghi (slot chống spam Telegram, bản chụp thời tiết, cờ job chờ) mới
dùng `IrqLock` vì được đọc/ghi từ hai task.

## Nhóm module (logic)

### Core
- `src/config.h` + `src/core/config.cpp` — hằng số cấu hình, chân GPIO, cờ phần cứng
- `src/globals.h` + `src/core/globals.cpp` — `SystemState` + đối tượng phần cứng
- `src/logger.h` + `src/core/logger.cpp` — log có mốc thời gian, mã lỗi, debug menu
- `src/settings.h` + `src/core/settings.cpp` — NVS persistence, chỉ ghi khóa thay đổi
- `src/secrets.h.example` — mẫu WiFi/token/OTA; `src/secrets.h` thật không commit

### HAL
- `src/hal.h` + `src/hal/hal.cpp` — truy cập phần cứng (GPIO, ADC, PWM, I2C, servo)

### Sensors
- `src/sensors.h` + `src/sensors/sensors.cpp` — đọc cảm biến, lọc trung bình động,
  failover DHT11, calibration

### Actuators
- `src/actuators.h` + `src/actuators/actuators.cpp` — relay, fan, LED, RGB, servo

### Control
- `src/control.h` + `src/control/control.cpp` — logic tự động (bơm, đèn, RGB, chạm, báo động)
- `src/demo.h` + `src/control/demo.cpp` — chế độ trình diễn (HĐ2–HĐ12)
- `src/pid.h` + `src/control/pid.cpp` — PID quạt, dùng khi `FAN_PWM_ENABLED=1`
- `src/softstart.h` + `src/control/softstart.cpp` — soft-start bơm (`SOFTSTART_MS`, mặc định 500 ms)
- `src/calibration.h` + `src/control/calibration.cpp` — offset/gain, kiểm tra hữu hạn/khoảng, NVS
- `src/anomaly.h` + `src/control/anomaly.cpp` — z-score; không ghi đè cờ health của DHT

### Connectivity
- `src/wifi.h` + `src/connectivity/wifi.cpp` — WiFi reconnect + `wifiConnected()`
- `src/webserver.h` + `src/connectivity/webserver.cpp` — HTTP + UI + `webGuard()`/`webTokenGuard()`
- `src/mqtt.h` + `src/connectivity/mqtt.cpp` — MQTT client (PubSubClient, mặc định tắt;
  LWT `smartfarm/status`, publish `smartfarm/state`, nhận `smartfarm/cmd`)
- `src/history.h` + `src/connectivity/history.cpp` — vòng đệm 96 mẫu trong RAM + `/history`
- `src/weather.h` + `src/connectivity/weather.cpp` — Open-Meteo + parser + `/weather`
- `src/notify.h` + `src/connectivity/notify.cpp` — Telegram Bot API, chống spam
- `src/connectivity/networker.h` + `.cpp` — queue + task mạng nền cho Telegram/thời tiết
- `src/irqlock.h` — spinlock `portENTER_CRITICAL` cho dữ liệu chia sẻ giữa loopTask và worker
- `src/ota.h` + `src/connectivity/ota.cpp` — hook OTA; `ArduinoOTA.begin()` được gọi
  từ `SmartFarm.ino` chỉ khi `OTA_PASSWORD` khác rỗng

### UI
- `src/oled.h` + `src/ui/oled.cpp` — màn hình OLED (dirty flag, force redraw)

### Observability
- `src/metrics.h` + `src/observability/metrics.cpp` — metrics JSON + `handleMetrics()`
- `src/health.h` + `src/observability/health.cpp` — health check + `handleCalibrate()`

## Cờ phần cứng (config.h)
- `MQTT_ENABLED` (mặc định 0), `MQTT_USE_TLS` (mặc định 0)
- `BLUETOOTH_ENABLED` (mặc định 0), `FAN_PWM_ENABLED` (mặc định 0)
- `SOFTSTART_MS`, `WINDMILL_MOTOR_PIN`, `WINDMILL_SERVO_SIM`
- `HISTORY_CAPACITY`, `HISTORY_INTERVAL_MS`, `WEATHER_LATITUDE`, `WEATHER_LONGITUDE`
- `BUZZER_ENABLED`, `RELAY_ACTIVE_LOW`, `FAN_ACTIVE_LOW`
- `INVERT_LIGHT`, `INVERT_SOIL`, `INVERT_WATER`
- `WEB_TOKEN`, `OTA_PASSWORD`, `ANTI_REPLAY`
- `WIFI_SSID`, `WIFI_PASSWORD` (mặc định rỗng, lấy từ `src/secrets.h`)

Bật `MQTT_ENABLED` cần cài thêm PubSubClient; không cần thư viện WebSocket/TLS ngoài
(transport TLS nằm trong core qua `WiFiClientSecure`). Bluetooth mặc định **tắt** để
firmware ~1,22 MB vừa partition 1,31 MB; bật Bluetooth (~1,9 MB) buộc dùng partition
`huge_app` và **mất OTA** (chỉ một slot app). Telegram mặc định không xác thực chứng
chỉ (`TELEGRAM_TLS_INSECURE=1`, có log ERROR nhắc); MQTT mặc định
`MQTT_TLS_INSECURE=0` — thiếu `MQTT_CA_CERT` thì từ chối kết nối (mã 601).

## Trạng thái toàn cục (SystemState)
- `sensorHealthy`, `sensorFailCount` — sức khỏe cảm biến do driver DHT quản lý
- `calTempOffset`, `calTempGain`, `calHumOffset`, `calHumGain`,
  `calLightOffset`, `calSoilOffset`, `calWaterOffset` — calibration
- `pidKp`, `pidKi`, `pidKd`, `pidIntegral`, `pidPrevError`, `pidLastTime` — PID quạt
- `anomalyTemp`, `anomalyHum`, `sensorHealthyTemp`, `sensorHealthyHum` — anomaly
- `powerSave`, `sleepSeconds` — deep sleep

## Bảo mật
- Mọi lệnh đổi trạng thái đi qua `webGuard()` (CSRF + token + anti-replay tùy chọn).
- `/health`, `/metrics`, `/calibrate`, `/history`, `/weather` kiểm tra token khi
  `WEB_TOKEN` được đặt; `/config` và `/all` của app ACEBOTT chỉ đọc (không đổi trạng thái).
- `/door` và `/led` của app dùng `guardWeb()`; mọi lệnh app tạm dừng automation
  3 giây nhưng không vô hiệu hóa luật an toàn nước.
- `/calibrate` chỉ lưu NVS khi sensor hợp lệ và offset hữu hạn/đúng khoảng.
- WiFi/mật khẩu/token/OTA nằm ở `src/secrets.h`, không hard-code trong repo.
- OTA chỉ bật khi `OTA_PASSWORD` khác rỗng.
