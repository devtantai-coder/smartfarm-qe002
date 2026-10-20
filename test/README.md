# Bộ test chuyên sâu chạy trên máy tính

Thư mục này biên dịch **chính firmware thật** (`SmartFarm.ino` + `src/*.cpp`) bằng
`g++` trên PC, thay các thư viện ESP32 bằng mock điều khiển được trong `mocks/`,
rồi chạy toàn bộ logic như trên kit — không cần phần cứng, không cần toolchain ESP32.

## Chạy test

```bash
./test/run_tests.sh --quick      # 1 bản build nhanh (core 3.x, cấu hình kit)
./test/run_tests.sh --full       # 6 bản build (mặc định khi không truyền tham số)
./test/run_tests.sh --coverage   # đo độ phủ dòng + nhánh bằng gcov (cũng đặt WEB_TOKEN)
```

Tùy biến bằng biến môi trường:

- `COVERAGE_MIN` — ngưỡng độ phủ dòng (mặc định 80), `--coverage` exit khác 0 nếu dưới.
- `BRANCH_MIN` — ngưỡng độ phủ nhánh "taken at least once" (mặc định 75).
- `FUZZ_ITERS` — số vòng fuzz mỗi seed (mặc định 30000; rút ngắn khi phát triển,
  ví dụ `FUZZ_ITERS=2000 ./test/run_tests.sh --quick`).

6 bản build trong chế độ `--full`:

| Tên | Cờ biên dịch | Mục đích |
|---|---|---|
| `core3` | `-DESP_ARDUINO_VERSION_MAJOR=3` | Cấu hình mặc định của kit trên core 3.x |
| `core2-invert` | `-DESP_ARDUINO_VERSION_MAJOR=2 -DINVERT_LIGHT=1 -DINVERT_SOIL=1 -DINVERT_WATER=1` | Tương thích core 2.x + cảm biến đảo chiều |
| `passive-lowrelay` | `-DBUZZER_PASSIVE=1 -DBUZZER_TONE_HZ=2000 -DRELAY_ACTIVE_LOW=1` | Còi passive (tone) + relay kích mức thấp |
| `asan+ubsan` | `-fsanitize=address,undefined -fno-omit-frame-pointer` | Phát hiện tràn bộ nhớ/UB; script tự bỏ `LD_PRELOAD` khi chạy |
| `webtoken` | `-DWEB_TOKEN='"token-bao-mat-test"'` | Bật `WEB_TOKEN` để chạy D16 (bảo vệ token web) |
| `tinyjson` | `-DAPI_JSON_BUF_SIZE=64` | Buffer JSON `/api` cố ý nhỏ để chạy D21 (nhánh "JSON bị cắt" → phải trả lỗi 500) |

Yêu cầu: `g++` hỗ trợ C++17 và `bash`; `node` (tuỳ chọn) cho bộ test JS.
Kết quả hiện tại: **772.537 lượt kiểm tra, 0 lỗi** trên 4 bản build thường; bản
`webtoken` và bản `--coverage` (cũng đặt `WEB_TOKEN`) chạy **772.554 lượt**; bản
`tinyjson` chạy 772.314 lượt (bỏ các kiểm tra nội dung JSON vì buffer cố ý
nhỏ). Độ phủ dòng lệnh: **99,8%** (2 dòng chưa phủ là nhánh "JSON bị cắt" —
chỉ chạy ở bản build `tinyjson`), độ phủ nhánh (taken at least once): **~80%**
cho toàn bộ code production — phần thiếu chủ yếu là các nhánh phòng vệ một
chiều (`errno` của thư viện, `else` bất khả thi, nhánh compile-time của
ternary). Mỗi test in một dòng `[PASS]/[FAIL]` kèm tên và số check của riêng nó
để khoanh vùng lỗi nhanh.

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
  - **I1–I13** tích hợp: boot đăng ký đủ 15 route, `/api`, `/on` `/off`, phân xử
    servo, các handler còn lại, trang chủ + favicon, cấu hình WiFi động qua
    `/wifi` (lưu flash + restart), callback OTA in trạng thái ra serial, OLED,
    menu serial 1–8, phím `9` chạy demo, WiFi mất–nối lại; I11 boot suy giảm
    (thiếu OLED + hết hạn WiFi, kiểm tra mã lỗi 101/102 và WDT vẫn được cho ăn).
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
    chọn (thiếu/sai → 401, đúng qua header `X-Auth-Token` hoặc `?token=` → 200,
    `/api` cũng bị chặn khi thiếu token; bản build không đặt `WEB_TOKEN` tự bỏ qua).
- `mocks/` — giả lập Arduino/ESP32: `mock.h` + `mock.cpp` (API điều khiển thời gian
  `mockSetMillis`, ADC, chân, log GPIO, OLED theo dòng, WiFi, web server kèm
  `mockServerSetHeader` giả lập header HTTP, counters)
  và các header thay thế `Arduino.h`, `WiFi.h`, `WebServer.h`, `Adafruit_SSD1306.h`,
  `Adafruit_NeoPixel.h`, `ESP32Servo.h`, `DHT.h`, `Preferences.h`, `Wire.h`,
  `esp_task_wdt.h`, `esp_arduino_version.h`.
  Bộ đệm OLED giả mô phỏng đúng bố cục 6 cột/ký tự của font 6×8 thật, nên phép
  diff theo dải cột của firmware được kiểm chứng đúng (đổi 1 ký tự → 6 byte
  I2C, không phải 128 byte của cả page).
- `js/page_test.mjs` — kiểm thử JavaScript của trang web bằng Node: **trích đúng
  đoạn `<script>` từ `src/webserver.cpp`** nên kiểm thử chính mã đang chạy trên
  thiết bị, không chép lại. Kiểm tra: poll thích ứng (2 s khi có thay đổi → 5 s
  khi đứng yên), token đọc từ URL → `sessionStorage` → header, tự lấy nonce mới
  khi gặp 403, mọi request có `AbortSignal` để không treo, không gọi chồng request.
  Chạy: `node test/js/page_test.mjs` (tự bỏ qua nếu máy không có `node`).
- `run_tests.sh` — script build + chạy (và chế độ coverage).
- `build/` — thư mục build tạm (đã `.gitignore`).
