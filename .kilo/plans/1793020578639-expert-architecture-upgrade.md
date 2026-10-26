# Smart Farm QE002 — Nâng cấp Kiến trúc Chuyên gia (Expert-Level)

## 1. Mục tiêu

Chuyển dự án từ kiến trúc Arduino bare-metal `loop()` đơn luồng sang kiến trúc chuyên gia đa task, với kết nối an toàn (TLS/MQTT/WebSocket), điều khiển thông minh (calibration/PID/anomaly), và kiểm thử chuyên sâu (HIL/contract/fuzz/chaos).

## 2. Quyết định thiết kế (đã xác nhận)

| # | Quyết định | Kết quả |
|---|-----------|---------|
| 1 | Mô hình runtime | FreeRTOS multi-tasking (SensorTask, ControlTask, CommTask, DisplayTask, PowerTask) |
| 2 | Giao tiếp task | Queue (dữ liệu cảm biến) + Event Group (sự kiện) + Mutex (SystemState) |
| 3 | Chia sẻ dữ liệu | `SystemState` duy nhất + mutex + snapshot đọc |
| 4 | Bảo mật & kết nối | TLS/WiFiClientSecure + MQTT (mqtts) + WebSocket + OTA ký mã + session management |
| 5 | Điều khiển thông minh | Hysteresis base + calibration + adaptive threshold + anomaly detection + soft-start pump + PID quạt |
| 6 | Kiểm thử | Mock FreeRTOS + HIL + contract testing + fuzz mở rộng + chaos testing |

## 3. Kiến trúc mới

### 3.1. Sơ đồ task

```
┌─────────────────────────────────────────────────────────────┐
│                    FreeRTOS Scheduler                      │
├──────────┬──────────┬──────────┬──────────┬───────────────┤
│ Sensor   │ Control  │ Comm     │ Display  │ Power         │
│ Task     │ Task     │ Task     │ Task     │ Task          │
│ (1Hz)    │ (10ms)   │ (event)  │ (2s)     │ (on-demand)   │
├──────────┼──────────┼──────────┼──────────┼───────────────┤
│ DHT11    │ Pump     │ WiFi     │ OLED     │ Deep sleep    │
│ LDR      │ Fan PID  │ HTTP     │          │ Timer wakeup  │
│ Soil     │ Light    │ MQTT     │          │ Ext wakeup    │
│ Water    │ RGB      │ WebSocket│          │               │
│ PIR      │ Breathe  │ OTA      │          │               │
│ Touch    │ Servo    │          │          │               │
└──────────┴──────────┴──────────┴──────────┴───────────────┘
         │           │           │           │
         ▼           ▼           ▼           ▼
    ┌─────────────────────────────────────────────┐
    │         SystemState (mutex-protected)       │
    └─────────────────────────────────────────────┘
```

### 3.2. Luồng dữ liệu

1. **SensorTask** đọc cảm biến mỗi `SENSOR_INTERVAL` (1s), lọc trung bình động, ghi vào `SystemState` qua mutex, gửi struct `SensorData` qua queue đến ControlTask.
2. **ControlTask** nhận `SensorData` từ queue, chạy logic điều khiển (pump/fan/PID/light/RGB/breathe/touch/alerts/windmill/feed), ghi trạng thái thiết bị vào `SystemState` qua mutex.
3. **CommTask** xử lý WiFi reconnect, HTTP request, MQTT publish/subscribe, WebSocket broadcast, OTA. Khi nhận lệnh từ web/MQTT, gửi event đến ControlTask qua event group.
4. **DisplayTask** đọc snapshot `SystemState` mỗi `OLED_INTERVAL` (2s), hoặc ngay khi `oledDirty` event được set, vẽ OLED.
5. **PowerTask** theo dõi cờ `powerSave`, khi bật thì gửi signal deep sleep đến tất cả task qua event group, rồi gọi `esp_deep_sleep_start()`.

### 3.3. Event Groups

| Bit | Tên | Mô tả |
|-----|-----|-------|
| 0 | `EVT_SENSOR_READY` | Cảm biến mới vừa đọc xong |
| 1 | `EVT_COMMAND` | Lệnh từ web/MQTT/Serial mới đến |
| 2 | `EVT_OLED_DIRTY` | Cần vẽ lại OLED ngay |
| 3 | `EVT_POWER_SAVE` | Vào deep sleep |
| 4 | `EVT_OTA_START` | Bắt đầu OTA update |
| 5 | `EVT_DEMO_RUNNING` | Demo đang chạy |

### 3.4. Queue

| Queue | From → To | Payload | Size |
|-------|-----------|---------|------|
| `qSensorData` | SensorTask → ControlTask | `SensorData` struct | 4 |
| `qCommand` | CommTask → ControlTask | `Command` struct | 8 |
| `qDisplayUpdate` | ControlTask → DisplayTask | `DisplaySnapshot` struct | 2 |

## 4. Module mới cần tạo

### 4.1. Kernel abstraction layer

| File | Mô tả |
|------|-------|
| `src/rtos.h` / `src/rtos.cpp` | Wrapper FreeRTOS: task create, mutex, queue, event group, watchdog per-task |
| `src/rtos_config.h` | Stack size, priority, watchdog timeout cho từng task |

### 4.2. Kết nối & bảo mật

| File | Mô tả |
|------|-------|
| `src/mqtt.h` / `src/mqtt.cpp` | MQTT client (PubSubClient), publish sensor data, subscribe commands, TLS |
| `src/websocket.h` / `src/websocket.cpp` | WebSocket server (WebSocketsServer), broadcast real-time, receive commands |
| `src/tls.h` / `src/tls.cpp` | TLS certificate management, WiFiClientSecure wrapper |
| `src/ota.h` / `src/ota.cpp` | OTA với signature verification (mbedtls) |

### 4.3. Điều khiển thông minh

| File | Mô tả |
|------|-------|
| `src/calibration.h` / `src/calibration.cpp` | Calibrate cảm biến: raw → physical, lưu hệ số offset/gain vào NVS |
| `src/anomaly.h` / `src/anomaly.cpp` | Phát hiện bất thường: z-score, rate-of-change, sensor health scoring |
| `src/pid.h` / `src/pid.cpp` | PID controller cho quạt (PWM output) |
| `src/softstart.h` / `src/softstart.cpp` | Soft-start pump: PWM ramp-up |

### 4.4. Observability

| File | Mô tả |
|------|-------|
| `src/metrics.h` / `src/metrics.cpp` | Structured metrics: task stack usage, queue depth, sensor health, uptime |
| `src/health.h` / `src/health.cpp` | Health check endpoint `/health`, task liveness |

## 5. Module cần sửa

### 5.1. `src/globals.h` / `src/globals.cpp`

- Thêm `SensorData`, `Command`, `DisplaySnapshot` struct
- Thêm mutex handle `sysMutex`
- Thêm queue/event group handles
- Thêm task handles
- Thêm calibration coefficients vào `SystemState`
- Thêm PID state, anomaly scores

### 5.2. `SmartFarm.ino`

- Thay `loop()` bằng FreeRTOS task setup: `setup()` tạo tasks, `loop()` chỉ feed WDT và monitor tasks
- `runAutomation()` tách thành ControlTask body

### 5.3. `src/sensors.h` / `src/sensors.cpp`

- Thêm calibration coefficients vào mỗi hàm đọc cảm biến
- Thêm anomaly detection trước khi ghi vào `SystemState`
- Thêm `readRaw*()` hàm để calibration

### 5.4. `src/control.h` / `src/control.cpp`

- Thêm `controlFanPid()` thay thế on/off fan
- Thêm `controlPumpSoftStart()`
- Thêm adaptive threshold logic
- Thêm anomaly-aware control (skip nếu sensor unhealthy)

### 5.5. `src/webserver.h` / `src/webserver.cpp`

- Thêm WebSocket endpoint
- Thêm `/health` endpoint
- Thêm `/calibrate` endpoint
- Thêm MQTT config endpoint
- Thêm TLS cert upload endpoint

### 5.6. `src/settings.h` / `src/settings.cpp`

- Thêm calibration coefficients lưu flash
- Thêm MQTT broker config lưu flash
- Thêm TLS cert storage (Secure Element hoặc NVS encrypted)

### 5.7. `src/wifi.h` / `src/wifi.cpp`

- Thêm WiFiClientSecure wrapper
- Thêm TLS certificate loading

### 5.8. `src/actuators.h` / `src/actuators.cpp`

- Thêm PWM fan control (PID output)
- Thêm pump soft-start ramp

### 5.9. `src/hal.h` / `src/hal.cpp`

- Thêm interface cho PWM fan, ADC raw, I2C, SPI
- Thêm mock-friendly abstraction (virtual functions hoặc function pointers)

## 6. Test infrastructure

### 6.1. Mock FreeRTOS

Tạo `test/mocks/`:
- `FreeRTOS.h` — mock task create, semaphore, queue, event group
- `queue.h` — mock xQueueSend/Receive
- `semphr.h` — mock xSemaphoreTake/Give
- `task.h` — mock xTaskCreate, vTaskDelay, xTaskGetTickCount
- `event_groups.h` — mock xEventGroupSetBits/WaitBits

### 6.2. Host test adjustments

- Mock FreeRTOS mutex thành no-op (single-threaded host)
- Mock queue thành ring buffer in-memory
- Mock event group thành bitmask
- Giữ nguyên logic test hiện có (772k+ checks)

### 6.3. HIL (Hardware-in-the-Loop)

- GitHub Actions self-hosted runner với ESP32 thực
- Test: ADC noise, I2C timing, WiFi reconnect thực tế, OTA flash
- Chạy nightly, không blocking PR

### 6.4. Contract testing

- JSON schema validation cho `/api` response (21 keys)
- MQTT message schema validation
- WebSocket frame schema validation

### 6.5. Fuzz expansion

- Tăng từ 3 seeds × 30k → 10 seeds × 100k
- Thêm fuzz cho MQTT payload, WebSocket frame, calibration input
- Thêm corpus từ test failures

### 6.6. Chaos testing

- Inject faults: sensor dropout, WiFi drop, NVS corrupt, task crash
- Verify recovery: watchdog restart, fallback mode, data integrity

## 7. CI/CD pipeline

```yaml
# .github/workflows/tests.yml
jobs:
  host-tests:        # g++ + mock FreeRTOS, 6 builds
  coverage:          # gcov, ngưỡng 80% dòng / 75% nhánh
  js-tests:          # node page_test.mjs
  hil-tests:         # self-hosted ESP32, nightly
  contract-tests:    # JSON schema validation
  fuzz-tests:        # 10 seeds × 100k steps
  chaos-tests:       # fault injection
```

## 8. Rollout / Migration path

### Phase 1: Foundation (FreeRTOS migration)
1. Tạo `src/rtos.h/cpp` wrapper
2. Tạo mock FreeRTOS cho host test
3. Chuyển `SmartFarm.ino` từ `loop()` sang task creation
4. Chuyển từng module: sensors → SensorTask, control → ControlTask, webserver → CommTask, oled → DisplayTask
5. Giữ `runAutomation()` như ControlTask body
6. Đảm bảo host test pass (772k+ checks)

### Phase 2: Connectivity (TLS/MQTT/WebSocket/OTA)
1. Thêm WiFiClientSecure wrapper
2. Thêm MQTT client + publish/subscribe
3. Thêm WebSocket server
4. Thêm OTA signature verification
5. Thêm session management
6. Thêm test cho mỗi module

### Phase 3: Smart control (calibration/PID/anomaly/soft-start)
1. Thêm calibration module + NVS storage
2. Thêm PID controller cho quạt
3. Thêm anomaly detection
4. Thêm soft-start pump
5. Thêm adaptive threshold
6. Thêm test cho mỗi module

### Phase 4: Observability (metrics/health)
1. Thêm metrics collection
2. Thêm `/health` endpoint
3. Thêm structured logging improvements
4. Thêm task monitoring

### Phase 5: Testing infrastructure
1. Thêm HIL test runner
2. Thêm contract tests
3. Thêm fuzz expansion
4. Thêm chaos tests
5. Cập nhật CI pipeline

## 9. Failure modes & mitigation

| Failure | Mitigation |
|---------|-----------|
| Task stack overflow | FreeRTOS stack monitoring, watchdog per task, increase stack size |
| Mutex deadlock | Timeout on xSemaphoreTake, deadlock detection, priority inheritance |
| Queue overflow | Drop oldest + log warning, increase queue size |
| Sensor failure | Anomaly detection, mark sensor unhealthy, skip control for that sensor |
| WiFi drop | Auto-reconnect, exponential backoff, MQTT reconnect |
| NVS corrupt | Validate on load, fallback to defaults, repair on write |
| Task crash | Watchdog restart, recovery mode |
| OTA failure | Rollback to previous firmware, signature verification |

## 10. Validation plan

1. **Host tests**: 772k+ checks pass trên 6 builds (core2, core3, invert, passive, asan, webtoken, tinyjson)
2. **FreeRTOS mock tests**: Task creation, mutex, queue, event group pass
3. **HIL tests**: ADC, I2C, WiFi, OTA pass trên ESP32 thật
4. **Contract tests**: JSON schema, MQTT schema, WebSocket schema pass
5. **Fuzz tests**: 10 seeds × 100k steps, 0 crashes
6. **Chaos tests**: Fault injection recovery verified
7. **Coverage**: ≥80% dòng, ≥75% nhánh trên toàn bộ code production
8. **Performance**: Task CPU usage <80%, queue depth <50%, response time <100ms

## 11. Open questions

1. **TLS certificate provisioning**: Self-signed vs Let's Encrypt? Trong LAN, self-signed là đủ; cho cloud, dùng Let's Encrypt. Cần quyết định trước Phase 2.
2. **MQTT broker**: Mosquitto local vs cloud broker (AWS IoT, Azure IoT Hub)? Cần quyết định trước Phase 2.
3. **Secure Element**: ATECC608A cho TLS key storage? Tùy chọn, tăng chi phí phần cứng. Cần quyết định trước Phase 2.
4. **PID tuning**: Cần dữ liệu thực tế để tune Kp/Ki/Kd. Có thể bắt đầu với giá trị mặc định rồi tune sau.
5. **HIL runner**: Cần ESP32 dev board + USB serial trên CI runner. Cần setup trước Phase 5.

## 12. Risks

| Risk | Impact | Mitigation |
|------|--------|-----------|
| FreeRTOS migration phá vỡ host test | Cao | Mock FreeRTOS trước, incremental migration, giữ test pass mỗi phase |
| TLS tăng memory usage | Trung bình | Partition scheme lớn hơn, monitor heap usage |
| MQTT/WebSocket tăng complexity | Trung bình | Modular design, test coverage cao |
| HIL test flaky | Thấp | Retry mechanism, nightly only, không blocking PR |
| PID tuning khó | Thấp | Bắt đầu với P-only, tune sau với dữ liệu thực |
