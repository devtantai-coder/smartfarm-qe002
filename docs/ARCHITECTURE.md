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
├── connectivity/       # wifi.cpp, webserver.cpp, mqtt.cpp, websocket.cpp, tls.cpp, ota.cpp
├── ui/                 # oled.cpp
└── observability/      # metrics.cpp, health.cpp
```

Các file `.h` nằm ở `src/` gốc; file `.cpp` tương ứng nằm trong thư mục module.
Mỗi `.cpp` include header bằng đường dẫn tương đối `../<module>.h`.

## Runtime: một vòng loop duy nhất

Firmware chạy **single-loop superloop**, không tạo task/queue/mutex FreeRTOS:

1. `setup()` khởi tạo watchdog, GPIO, cảm biến, OLED, NVS, WiFi, OTA (chỉ khi có
   mật khẩu), web server và các module.
2. `loop()` gọi lần lượt:
   - `server.handleClient()` — phục vụ HTTP;
   - `manageWiFi()` — kết nối lại WiFi không chặn;
   - `mqttLoop()`, `websocketLoop()`, `otaLoop()` — chỉ hoạt động khi module
     tương ứng được bật;
   - `ArduinoOTA.handle()` mỗi `OTA_HANDLE_INTERVAL`;
   - xử lý Serial/Bluetooth, lệnh debug và hẹn giờ khởi động lại;
   - `runAutomation()`;
   - vẽ OLED khi `oledDirty` hoặc đến `OLED_INTERVAL`;
   - deep sleep nếu `powerSave` được bật.

`runAutomation()` là nguồn logic duy nhất cho cả firmware và test host:
- đọc touch/PIR mỗi vòng;
- đọc DHT11/ADC mỗi `SENSOR_INTERVAL` (1 giây), lọc trung bình động, áp calibration;
- cập nhật anomaly score trên mẫu vừa lọc;
- kiểm tra an toàn bơm khô;
- chạy control/demo hoặc cập nhật demo.

Nhờ single-loop, web handler, cảm biến và actuator dùng chung `SystemState` mà
không cần khóa; production và bộ test chạy đúng cùng một đường logic.

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
- `src/pid.h` + `src/control/pid.cpp` — PID quạt (module mở rộng, chưa dùng trong control hiện tại)
- `src/softstart.h` + `src/control/softstart.cpp` — soft-start bơm (module mở rộng, chưa dùng)
- `src/calibration.h` + `src/control/calibration.cpp` — offset/gain, kiểm tra hữu hạn/khoảng, NVS
- `src/anomaly.h` + `src/control/anomaly.cpp` — z-score; không ghi đè cờ health của DHT

### Connectivity
- `src/wifi.h` + `src/connectivity/wifi.cpp` — WiFi reconnect + `wifiConnected()`
- `src/webserver.h` + `src/connectivity/webserver.cpp` — HTTP + UI + `webGuard()`/`webTokenGuard()`
- `src/mqtt.h` + `src/connectivity/mqtt.cpp` — MQTT client (mặc định tắt)
- `src/websocket.h` + `src/connectivity/websocket.cpp` — WebSocket broadcast (mặc định tắt)
- `src/tls.h` + `src/connectivity/tls.cpp` — placeholder; `TLS_ENABLED=1` chỉ cảnh báo,
  chưa triển khai transport TLS
- `src/ota.h` + `src/connectivity/ota.cpp` — OTA; `otaVerifySignature()` hiện trả `false`
  (chưa triển khai chữ ký số)

### UI
- `src/oled.h` + `src/ui/oled.cpp` — màn hình OLED (dirty flag, force redraw)

### Observability
- `src/metrics.h` + `src/observability/metrics.cpp` — metrics JSON + `handleMetrics()`
- `src/health.h` + `src/observability/health.cpp` — health check + `handleCalibrate()`

## Cờ phần cứng (config.h)
- `TLS_ENABLED`, `MQTT_ENABLED`, `WEBSOCKET_ENABLED` (mặc định 0)
- `BLUETOOTH_ENABLED`, `BUZZER_ENABLED`, `RELAY_ACTIVE_LOW`, `FAN_ACTIVE_LOW`
- `INVERT_LIGHT`, `INVERT_SOIL`, `INVERT_WATER`
- `WEB_TOKEN`, `OTA_PASSWORD`, `ANTI_REPLAY`
- `WIFI_SSID`, `WIFI_PASSWORD` (mặc định rỗng, lấy từ `src/secrets.h`)

Bật `MQTT_ENABLED`/`WEBSOCKET_ENABLED` cần cài thêm PubSubClient/WebSockets.
`TLS_ENABLED` chưa có transport TLS thực.

## Trạng thái toàn cục (SystemState)
- `sensorHealthy`, `sensorFailCount` — sức khỏe cảm biến do driver DHT quản lý
- `calTempOffset`, `calTempGain`, `calHumOffset`, `calHumGain`,
  `calLightOffset`, `calSoilOffset`, `calWaterOffset` — calibration
- `pidKp`, `pidKi`, `pidKd`, `pidIntegral`, `pidPrevError`, `pidLastTime` — PID quạt
- `anomalyTemp`, `anomalyHum`, `sensorHealthyTemp`, `sensorHealthyHum` — anomaly
- `powerSave`, `sleepSeconds` — deep sleep

## Bảo mật
- Mọi lệnh đổi trạng thái đi qua `webGuard()` (CSRF + token + anti-replay tùy chọn).
- `/health`, `/metrics`, `/calibrate` kiểm tra token khi `WEB_TOKEN` được đặt.
- `/calibrate` chỉ lưu NVS khi sensor hợp lệ và offset hữu hạn/đúng khoảng.
- WiFi/mật khẩu/token/OTA nằm ở `src/secrets.h`, không hard-code trong repo.
- OTA chỉ bật khi `OTA_PASSWORD` khác rỗng.
