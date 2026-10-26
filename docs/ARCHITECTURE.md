# Kiến trúc module Smart Farm QE002

## Cấu trúc thư mục

```
src/
├── *.h                 # header công khai ở gốc src/ (config, globals, logger, ...)
├── core/               # config.cpp, globals.cpp, logger.cpp, settings.cpp, rtos.cpp
├── hal/                # hal.cpp
├── sensors/            # sensors.cpp
├── actuators/          # actuators.cpp
├── control/            # control.cpp, demo.cpp, pid.cpp, softstart.cpp, calibration.cpp, anomaly.cpp
├── connectivity/       # wifi.cpp, webserver.cpp, mqtt.cpp, websocket.cpp, tls.cpp, ota.cpp
├── ui/                 # oled.cpp
└── observability/      # metrics.cpp, health.cpp
```

Ghi chú: các file `.h` nằm ở `src/` gốc; file `.cpp` tương ứng nằm trong thư mục module.
Mỗi `.cpp` include header bằng đường dẫn tương đối `../<module>.h`. Khi build host, cờ `-I`
trỏ cả `src/` và từng thư mục con.

## Nhóm module (logic)

### Core
- `src/config.h` + `src/core/config.cpp` — hằng số cấu hình, chân GPIO, cờ phần cứng
- `src/globals.h` + `src/core/globals.cpp` — `SystemState` + đối tượng phần cứng + mutex
- `src/logger.h` + `src/core/logger.cpp` — log có mốc thời gian, mã lỗi, debug menu
- `src/settings.h` + `src/core/settings.cpp` — NVS persistence
- `src/rtos.h` + `src/core/rtos.cpp` — wrapper FreeRTOS (mutex đệ quy, queue, event group)

### HAL
- `src/hal.h` + `src/hal/hal.cpp` — truy cập phần cứng (GPIO, ADC, PWM, I2C, servo)

### Sensors
- `src/sensors.h` + `src/sensors/sensors.cpp` — đọc cảm biến, lọc trung bình động, calibration

### Actuators
- `src/actuators.h` + `src/actuators/actuators.cpp` — relay, fan, LED, RGB, servo

### Control
- `src/control.h` + `src/control/control.cpp` — logic tự động (bơm, đèn, RGB, chạm, báo động)
- `src/demo.h` + `src/control/demo.cpp` — chế độ trình diễn (HĐ2–HĐ12)
- `src/pid.h` + `src/control/pid.cpp` — PID quạt
- `src/softstart.h` + `src/control/softstart.cpp` — soft-start bơm
- `src/calibration.h` + `src/control/calibration.cpp` — hiệu chỉnh cảm biến (offset/gain, NVS)
- `src/anomaly.h` + `src/control/anomaly.cpp` — phát hiện bất thường (z-score, health score)

### Connectivity
- `src/wifi.h` + `src/connectivity/wifi.cpp` — WiFi reconnect + `wifiConnected()`
- `src/webserver.h` + `src/connectivity/webserver.cpp` — HTTP + UI
- `src/mqtt.h` + `src/connectivity/mqtt.cpp` — MQTT client
- `src/websocket.h` + `src/connectivity/websocket.cpp` — WebSocket broadcast + counter
- `src/tls.h` + `src/connectivity/tls.cpp` — TLS
- `src/ota.h` + `src/connectivity/ota.cpp` — OTA

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
