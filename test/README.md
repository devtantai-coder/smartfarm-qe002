# Bộ test chuyên sâu chạy trên máy tính

Thư mục này biên dịch **chính firmware thật** (`SmartFarm.ino` + `src/*.cpp`) bằng
`g++` trên PC, thay các thư viện ESP32 bằng mock điều khiển được trong `mocks/`,
rồi chạy toàn bộ logic như trên kit — không cần phần cứng, không cần toolchain ESP32.

## Chạy test

```bash
./test/run_tests.sh --quick      # 1 bản build nhanh (core 3.x, cấu hình kit)
./test/run_tests.sh --full       # 5 bản build (mặc định khi không truyền tham số)
./test/run_tests.sh --coverage   # đo độ phủ bằng gcov (cũng đặt WEB_TOKEN; ngưỡng COVERAGE_MIN, mặc định 80%)
```

5 bản build trong chế độ `--full`:

| Tên | Cờ biên dịch | Mục đích |
|---|---|---|
| `core3` | `-DESP_ARDUINO_VERSION_MAJOR=3` | Cấu hình mặc định của kit trên core 3.x |
| `core2-invert` | `-DESP_ARDUINO_VERSION_MAJOR=2 -DINVERT_LIGHT=1 -DINVERT_SOIL=1 -DINVERT_WATER=1` | Tương thích core 2.x + cảm biến đảo chiều |
| `passive-lowrelay` | `-DBUZZER_PASSIVE=1 -DBUZZER_TONE_HZ=2000 -DRELAY_ACTIVE_LOW=1` | Còi passive (tone) + relay kích mức thấp |
| `asan+ubsan` | `-fsanitize=address,undefined -fno-omit-frame-pointer` | Phát hiện tràn bộ nhớ/UB; script tự bỏ `LD_PRELOAD` khi chạy |
| `webtoken` | `-DWEB_TOKEN='"token-bao-mat-test"'` | Bật `WEB_TOKEN` để chạy D16 (bảo vệ token web) |

Yêu cầu: `g++` hỗ trợ C++17 và `bash`. Kết quả hiện tại: **772.318 lượt kiểm tra,
0 lỗi** trên 4 bản build thường; bản `webtoken` và bản `--coverage` (cũng đặt
`WEB_TOKEN`) chạy **772.331 lượt**. Độ phủ dòng lệnh: **99,8%** (`logger.cpp` 99,1%,
`SmartFarm.ino` 99,3%, các file còn lại 100%).

## Cấu trúc

- `test_main.cpp` — toàn bộ test:
  - **T1–T17** đơn vị: cảm biến, bơm (trễ + an toàn khô), quạt, đèn/PIR/báo động,
    LED thường/thở, chạm chống dội, RGB, còi, cối xay gió, cửa cho ăn, thời tiết,
    settings, trình diễn; T16 quyền sở hữu đèn của PIR + khôi phục đèn thở, T17
    khôi phục đèn thở sau trình diễn.
  - **D1–D3** biên: so sánh ngưỡng đúng dấu `<`/`>` tại đúng giá trị biên, tính
    bất biến (idempotency) của cơ cấu chấp hành, thời gian chính xác từng mili-giây
    (chống dội 300 ms, cửa cho ăn 2000 ms, nhịp thở 30 ms, quét servo 30 ms).
  - **I1–I11** tích hợp: boot đăng ký đủ 14 route, `/api`, `/on` `/off`, phân xử
    servo, các handler còn lại, trang chủ + favicon, OLED, menu serial 1–8, phím
    `9` chạy demo, WiFi mất–nối lại; I11 boot suy giảm (thiếu OLED + hết hạn
    WiFi, kiểm tra mã lỗi 101/102 và WDT vẫn được cho ăn).
  - **D5–D16** chuyên sâu: hợp đồng JSON 21 khóa (parser tự viết, 200 trạng thái
    ngẫu nhiên), thân phản hồi chính xác + ngưỡng biên, layout OLED từng dòng
    (giới hạn 21 ký tự), biên serial (`\r\n`, byte rác 0xFF, chế độ debug tạm dừng
    tự động hóa rồi khôi phục an toàn), chẩn đoán demo (đúng 15 bước, tổng
    30.700 ms, an toàn khi hết nước giữa chừng), chống spam WiFi (đúng 10.000 ms),
    thứ tự khởi động (chốt đầu ra trước `pinMode`, WDT), bền vững sau mất điện,
    fuzz 3 seed × 30.000 bước với bất biến an toàn (bơm không chạy khô, chân khớp
    trạng thái, cối xay/cho ăn loại trừ nhau, góc servo 0–180...), tràn
    `millis()` cho mọi bộ đếm thời gian, chặn CSRF (Origin/Referer lạ hoặc
    `Sec-Fetch-Site: cross-site` → 403, cùng nguồn được phép), và token web tùy
    chọn (thiếu/sai → 401, đúng qua header `X-Auth-Token` hoặc `?token=` → 200;
    bản build không đặt `WEB_TOKEN` tự bỏ qua).
- `mocks/` — giả lập Arduino/ESP32: `mock.h` + `mock.cpp` (API điều khiển thời gian
  `mockSetMillis`, ADC, chân, log GPIO, OLED theo dòng, WiFi, web server kèm
  `mockServerSetHeader` giả lập header HTTP, counters)
  và các header thay thế `Arduino.h`, `WiFi.h`, `WebServer.h`, `Adafruit_SSD1306.h`,
  `Adafruit_NeoPixel.h`, `ESP32Servo.h`, `DHT.h`, `Preferences.h`, `Wire.h`,
  `esp_task_wdt.h`, `esp_arduino_version.h`.
- `run_tests.sh` — script build + chạy (và chế độ coverage).
- `build/` — thư mục build tạm (đã `.gitignore`).
