# Bộ test chuyên sâu chạy trên máy tính

Thư mục này biên dịch **chính firmware thật** (`SmartFarm.ino` + `src/*.cpp`) bằng
`g++` trên PC, thay các thư viện ESP32 bằng mock điều khiển được trong `mocks/`,
rồi chạy toàn bộ logic như trên kit — không cần phần cứng, không cần toolchain ESP32.

## Chạy test

```bash
./test/run_tests.sh --quick      # 1 bản build nhanh (core 3.x, cấu hình kit)
./test/run_tests.sh --full       # 8 bản build (mặc định khi không truyền tham số) + test JS
./test/run_tests.sh --extras     # chỉ bản PID/PWM + TLS + MQTT (khi phát triển)
./test/run_tests.sh --coverage   # đo độ phủ dòng + nhánh bằng gcov (bật WEB_TOKEN, FAN_PWM_ENABLED)
```

Tùy biến bằng biến môi trường:

- `COVERAGE_MIN` — ngưỡng độ phủ dòng (mặc định 80), `--coverage` exit khác 0 nếu dưới.
- `BRANCH_MIN` — ngưỡng độ phủ nhánh "taken at least once" (mặc định 75).
- `FUZZ_ITERS` — số vòng fuzz mỗi seed (mặc định 30000; rút ngắn khi phát triển,
  ví dụ `FUZZ_ITERS=2000 ./test/run_tests.sh --quick`).

8 bản build trong chế độ `--full` (đúng theo `test/run_tests.sh:73-100`):

| Tên | Cờ biên dịch | Mục đích |
|---|---|---|
| `core3` | `-DESP_ARDUINO_VERSION_MAJOR=3` | Cấu hình mặc định của kit trên core 3.x |
| `core2-invert` | `-DESP_ARDUINO_VERSION_MAJOR=2 -DINVERT_LIGHT=1 -DINVERT_SOIL=1 -DINVERT_WATER=1` | Tương thích core 2.x + cảm biến đảo chiều |
| `passive-lowrelay` | `-DESP_ARDUINO_VERSION_MAJOR=3 -DBUZZER_PASSIVE=1 -DRELAY_ACTIVE_LOW=1` | Còi passive (tone) + relay kích mức thấp |
| `asan+ubsan` | `-g -fsanitize=address,undefined -fno-omit-frame-pointer -DESP_ARDUINO_VERSION_MAJOR=3` | Phát hiện tràn bộ nhớ/UB; script tự bỏ `LD_PRELOAD` khi chạy |
| `webtoken` | `-DESP_ARDUINO_VERSION_MAJOR=3 -DWEB_TOKEN='"token-bao-mat-test"'` | Bật `WEB_TOKEN` để chạy D16 (bảo vệ token web) |
| `tinyjson` | `-DESP_ARDUINO_VERSION_MAJOR=3 -DAPI_JSON_BUF_SIZE=64` | Buffer JSON `/api` cố ý nhỏ để chạy D21 (nhánh "JSON bị cắt" → phải trả lỗi 500) |
| `extras` | `-DESP_ARDUINO_VERSION_MAJOR=3 -DFAN_PWM_ENABLED=1 -DMQTT_USE_TLS=1 -DMQTT_ENABLED=1` | PID/PWM quạt, MQTT + TLS và kiểm tra phần cứng PWM (D30, D32) |
| `replay` | `-DESP_ARDUINO_VERSION_MAJOR=3 -DANTI_REPLAY=1` | Bật `ANTI_REPLAY` (mặc định 0) để nhánh nonce trong `guardWeb()` được biên dịch và D34 thật sự chạy |

Yêu cầu: `g++` hỗ trợ C++17 và `bash`; `node` (tuỳ chọn) cho bộ test JS.
Mỗi bản build in dòng cuối `=== N checks, 0 failures ===` với số lượt kiểm tra
của riêng nó — hàng trăm nghìn lượt, phần lớn đến từ 3 seed × `FUZZ_ITERS` vòng
fuzz, không phải vài trăm nghìn kịch bản độc lập. Con số thay đổi theo cấu hình
build và theo code, nên không ghi cố định vào tài liệu; muốn biết chính xác hãy
chạy `./test/run_tests.sh --full` và xem dòng tổng kết của từng bản build. Độ phủ
dòng lệnh và độ phủ nhánh (taken at least once) lấy từ `./test/run_tests.sh
--coverage` (bản coverage bật `WEB_TOKEN` và `FAN_PWM_ENABLED` để phủ
các nhánh chỉ tồn tại khi bật cờ — D16, D30);
ngưỡng chặn CI là `COVERAGE_MIN=80` và `BRANCH_MIN=75`. Phần nhánh chưa phủ tập trung ở
đường mạng/TLS thật (bị biên dịch bỏ khi build host có `SMARTFARM_TEST_HOOKS`),
các nhánh lỗi khó mô phỏng (driver/HTTP trả sai), và biến thể compile-time không
bật trong bản coverage. Mỗi test in một
dòng `[PASS]/[FAIL]` kèm tên và số check của riêng nó để khoanh vùng lỗi nhanh.

## Cấu trúc

- `test_main.cpp` — toàn bộ test:
  - **T1–T19** đơn vị: cảm biến, bơm (trễ + an toàn khô), quạt, đèn/PIR/báo động,
    LED thường/thở, chạm chống dội, RGB, còi, cối xay gió, cửa cho ăn, thời tiết,
    settings, trình diễn; T16 quyền sở hữu đèn của PIR + khôi phục đèn thở, T17
    khôi phục đèn thở sau trình diễn, T18 reset mặc định + bỏ qua ghi flash khi
    giá trị không đổi + mã bảo mật không bị ghi vào NVS, T19 logger với cấp log
    ngoài enum.
  - **D1–D3** biên: so sánh ngưỡng đúng dấu `<`/`>` tại đúng giá trị biên, tính
    bất biến (idempotency) của cơ cấu chấp hành, thời gian chính xác từng mili-giây
    (chống dội 300 ms, cửa cho ăn 2000 ms, nhịp thở 30 ms, quét servo 30 ms).
  - **I1–I15** tích hợp: boot đăng ký đủ 25 route, `/api`, `/on` `/off`, phân xử
    servo, các handler còn lại, trang chủ + favicon, cấu hình WiFi động qua
    `/wifi` (lưu flash + restart), `/calibrate` (guard + validate + lưu NVS),
    `/health` + `/metrics`, callback OTA in trạng thái ra serial, OLED,
    menu serial 1–8, phím `9` chạy demo, WiFi mất–nối lại; I11 boot suy giảm
    (thiếu OLED + hết hạn WiFi, kiểm tra mã lỗi 101/102 và WDT vẫn được cho ăn).
  - **D5–D16** chuyên sâu: hợp đồng JSON 23 khóa (parser tự viết, 200 trạng thái
    ngẫu nhiên), thân phản hồi chính xác + ngưỡng biên, layout OLED từng dòng
    (giới hạn 21 ký tự), biên serial (`\r\n`, byte rác 0xFF, chế độ debug tạm dừng
    tự động hóa rồi khôi phục an toàn), chẩn đoán demo (đúng 15 bước, tổng
    30.700 ms, an toàn khi hết nước giữa chừng), chống spam WiFi (đúng 10.000 ms),
    thứ tự khởi động (chốt đầu ra trước `pinMode`, WDT), bền vững sau mất điện,
    fuzz 3 seed × 30.000 bước với bất biến an toàn (bơm không chạy khô, chân khớp
    trạng thái, cối xay/cho ăn loại trừ nhau, góc servo 0–180...), tràn
    `millis()` cho mọi bộ đếm thời gian, chặn CSRF (Origin/Referer lạ hoặc
    `Sec-Fetch-Site: cross-site` → 403, cùng nguồn được phép), và token web tùy
    chọn (thiếu/sai → 401, đúng qua header `X-Auth-Token` hoặc `?token=` → 200,
    `/api`, `/health`, `/metrics` cũng bị chặn khi thiếu token; bản build không đặt `WEB_TOKEN` tự bỏ qua).
  - **D22–D24** nhánh biên còn sót: CSRF cùng nguồn có cổng/query, `id` lạ →
    400, `/threshold` và `/wifi` không có tham số → 400, tự chỉnh ngưỡng khi
    `high <= low`, đổi riêng mật khẩu WiFi, token dài hơn giá trị thật → 401,
    nonce tràn 32 bit (vẫn trả nonce hợp lệ), OTA với `total = 0` và mã lỗi lạ,
    log bị chặn dưới ngưỡng, mọi route có `guardWeb()` phải trả 403 cho nguồn lạ,
    cờ debug in đúng chỗ, clamp ngưỡng khi flash lưu giá trị hỏng, RGB chỉ
    khác 0 ở G hoặc B, PIR/đèn thở giữ quyền điều khiển LED, DHT trả NaN khi
    bộ đệm còn trống.
  - **D25–D34** tính năng mới: vòng lịch sử 96 mẫu + JSON `/history` (ghi đè
    vòng, buffer nhỏ), parser Open-Meteo + mô tả WMO + `/weather`, endpoint app
    ACEBOTT (`/config`, `/all`, `/door`, `/led`) và tạm dừng automation 3 giây,
    soft-start bơm + hủy khi mất nước, động cơ cối xay gió GPIO16, PID/PWM quạt,
    Telegram (rate limit + payload qua hook), cờ MQTT/TLS theo cấu hình build,
    parser lệnh Bluetooth + đệm lệnh theo dòng 64 byte, và D34 cửa sổ chống replay
    (chỉ chạy ở bản build `replay`; các bản build khác bỏ qua vì `ANTI_REPLAY=0`).
- `mocks/` — giả lập Arduino/ESP32: `mock.h` + `mock.cpp` (API điều khiển thời gian
  `mockSetMillis`, ADC, chân, log GPIO, OLED theo dòng, WiFi, web server kèm
  `mockServerSetHeader` giả lập header HTTP, counters), BluetoothSerial giả lập
  (bơm lệnh vào `mockFeed`, đếm phản hồi, reset giữa test)
  và các header thay thế `Arduino.h`, `WiFi.h`, `WebServer.h`, `ArduinoOTA.h`,
  `Adafruit_SSD1306.h`, `Adafruit_NeoPixel.h`, `ESP32Servo.h`, `DHT.h`,
  `Preferences.h`, `Wire.h`, `esp_task_wdt.h`, `esp_arduino_version.h`, cùng nhóm
  header FreeRTOS (`FreeRTOS.h`, `task.h`, `queue.h`, `semphr.h`, `event_groups.h`)
  — `mock.cpp` cài `xTaskGetTickCount` và họ `xSemaphore*` để các nhánh cần khóa
  vẫn liên kết và chạy được trên host.
  Bộ đệm OLED giả mô phỏng đúng bố cục 6 cột/ký tự của font 6×8 thật, nên phép
  diff theo dải cột của firmware được kiểm chứng đúng (đổi 1 ký tự → 6 byte
  I2C, không phải 128 byte của cả page).
- `js/page_test.mjs` — kiểm thử JavaScript của trang web bằng Node: **trích đúng
  đoạn `<script>` từ `src/connectivity/webserver.cpp`** nên kiểm thử chính mã đang
  chạy trên thiết bị, không chép lại. Kiểm tra: poll thích ứng (2 s khi có thay đổi →
  khi đứng yên), token đọc từ URL → `sessionStorage` → header, tự lấy nonce mới
  khi gặp 403, mọi request có `AbortSignal` để không treo, không gọi chồng request.
  Chạy: `node test/js/page_test.mjs` (tự bỏ qua nếu máy không có `node`).
- `run_tests.sh` — script build + chạy (và chế độ coverage).
- `build/` — thư mục build tạm (đã `.gitignore`).
