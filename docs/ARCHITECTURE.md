# Kiến trúc module Smart Farm QE002

## Cấu trúc thư mục

```
src/
├── core/               # config.h/cpp, globals.h/cpp, logger.h/cpp, settings.h/cpp, rtos.h/cpp
├── hal/                # hal.h/cpp
├── sensors/            # sensors.h/cpp
├── actuators/          # actuators.h/cpp
├── control/            # control.h/cpp, demo.h/cpp, pid.h/cpp, softstart.h/cpp, calibration.h/cpp, anomaly.h/cpp
├── connectivity/       # wifi.h/cpp, webserver.h/cpp, mqtt.h/cpp, websocket.h/cpp, tls.h/cpp, ota.h/cpp
├── ui/                 # oled.h/cpp
└── observability/      # metrics.h/cpp, health.h/cpp
```

Ghi chú: mỗi thư mục module chứa cả `.h` và `.cpp` tương ứng. File `.cpp` include header cùng thư mục bằng tên file (ví dụ `#include "globals.h"`), cross-module qua đường dẫn tương đối `../<module>/<header>.h`. Khi build host, cờ `-I` trỏ `src/` (và `test/mocks/`).

## Nhóm module (logic)

### Core
- `src/core/config.h` + `src/core/config.cpp` — hằng số cấu hình, chân GPIO, cờ phần cứng
- `src/core/globals.h` + `src/core/globals.cpp` — `SystemState` + đối tượng phần cứng + mutex
- `src/core/logger.h` + `src/core/logger.cpp` — log có mốc thời gian, mã lỗi, debug menu
- `src/core/settings.h` + `src/core/settings.cpp` — NVS persistence
- `src/core/rtos.h` + `src/core/rtos.cpp` — wrapper FreeRTOS (mutex đệ quy, queue, event group)

### HAL
- `src/hal/hal.h` + `src/hal/hal.cpp` — truy cập phần cứng (GPIO, ADC, PWM, I2C, servo)

### Sensors
- `src/sensors/sensors.h` + `src/sensors/sensors.cpp` — đọc cảm biến, lọc trung bình động, calibration

### Actuators
- `src/actuators/actuators.h` + `src/actuators/actuators.cpp` — relay, fan, LED, RGB, servo

### Control
- `src/control/control.h` + `src/control/control.cpp` — logic tự động (bơm, đèn, RGB, chạm, báo động)
- `src/control/demo.h` + `src/control/demo.cpp` — chế độ trình diễn (HĐ2–HĐ12)
- `src/control/pid.h` + `src/control/pid.cpp` — PID quạt
- `src/control/softstart.h` + `src/control/softstart.cpp` — soft-start bơm
- `src/control/calibration.h` + `src/control/calibration.cpp` — hiệu chỉnh cảm biến (offset/gain, NVS)
- `src/control/anomaly.h` + `src/control/anomaly.cpp` — phát hiện bất thường (z-score, health score)

### Connectivity
- `src/connectivity/wifi.h` + `src/connectivity/wifi.cpp` — WiFi reconnect + `wifiConnected()`
- `src/connectivity/webserver.h` + `src/connectivity/webserver.cpp` — HTTP + UI
- `src/connectivity/mqtt.h` + `src/connectivity/mqtt.cpp` — MQTT client
- `src/connectivity/websocket.h` + `src/connectivity/websocket.cpp` — WebSocket broadcast + counter
- `src/connectivity/tls.h` + `src/connectivity/tls.cpp` — TLS
- `src/connectivity/ota.h` + `src/connectivity/ota.cpp` — OTA

### UI
- `src/ui/oled.h` + `src/ui/oled.cpp` — màn hình OLED (dirty flag, force redraw)

### Observability
- `src/observability/metrics.h` + `src/observability/metrics.cpp` — metrics JSON + `handleMetrics()`
- `src/observability/health.h` + `src/observability/health.cpp` — health check + `handleCalibrate()`

## Cờ phần cứng (config.h)
- `TLS_ENABLED`, `MQTT_ENABLED`, `WEBSOCKET_ENABLED` (mặc định 0)
- `BLUETOOTH_ENABLED`, `BUZZER_ENABLED`, `RELAY_ACTIVE_LOW`, `FAN_ACTIVE_LOW`
- `INVERT_LIGHT`, `INVERT_SOIL`, `INVERT_WATER`
- `WEB_TOKEN`, `OTA_PASSWORD`, `ANTI_REPLAY`

## Trạng thái toàn cục (SystemState)
- `sensorHealthy`, `sensorFailCount` — sức khỏe cảm biến
- `calTempOffset`, `calTempGain`, `calHumGain`, `calLightOffset`, `calSoilOffset`, `calWaterOffset` — calibration
- `pidKp`, `pidKi`, `pidKd`, `pidIntegral`, `pidPrevError`, `pidLastTime` — PID quạt
- `anomalyTemp`, `anomalyHum`, `sensorHealthyTemp`, `sensorHealthyHum` — anomaly
- `powerSave`, `sleepSeconds` — deep sleep

## Task FreeRTOS
- SensorTask, ControlTask, CommTask, DisplayTask, PowerTask

## Event bits
- `EVT_SENSOR_READY`, `EVT_COMMAND`, `EVT_OLED_DIRTY`, `EVT_POWER_SAVE`, `EVT_OTA_START`, `EVT_DEMO_RUNNING`

## Queue
- `qSensorData` (SensorTask → ControlTask), `qCommand` (CommTask → ControlTask), `qDisplayUpdate` (ControlTask → DisplayTask)
