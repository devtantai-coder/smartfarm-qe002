#include "../config.h"

// Các giá trị cấu hình có thể thay đổi lúc chạy (WiFi, ngưỡng, mã bảo mật)
// nằm trong SystemState — xem src/globals.h / globals.cpp.

// ── Thời gian ──
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
// WiFi.status() gọi vào driver WiFi (khóa toàn cục dùng chung với chính stack
// WiFi). Hỏi ở tần số của loop() sẽ tranh giành với stack WiFi mà không tăng
// giá trị gì: trạng thái kết nối thay đổi theo giây, không theo mili-giây.
const unsigned long WIFI_STATUS_POLL_MS = 250;
// Nhịp đọc cảm biến PHẢI >= MIN_INTERVAL của thư viện Adafruit DHT (2000 ms):
// đọc sớm hơn thì DHT::read() trả lại chính khung đã cache lần trước (không báo
// lỗi). Với 1000 ms, cứ hai mẫu thì một mẫu là bản sao -> cửa sổ trung bình
// SENSOR_FILTER_SIZE=5 thực chất chỉ còn ~2.5 mẫu độc lập, và cảm biến chết giữa
// chừng còn "hợp lệ" thêm 2 giây nên qua mặt sensorFailCount.
const unsigned long SENSOR_INTERVAL     = 2000;
const unsigned long OLED_INTERVAL       = 2000;
// ArduinoOTA.handle() đọc một gói UDP mỗi lần gọi. OTA không nhạy cảm với độ
// trễ nên gọi mỗi 20 ms là quá đủ, thay vì mỗi vòng loop (~1 kHz).
const unsigned long OTA_HANDLE_INTERVAL  = 20;
const unsigned long TOUCH_DEBOUNCE      = 300;
// Cửa sổ bỏ qua đầu vào số sau khi cấp điện. GPIO39/GPIO33 (chạm/PIR) là chân
// chỉ-input; khi cảm biến chưa cắm thì đường line trôi nổi và digitalRead() có
// thể trả HIGH ngay lúc khởi động. Không có cửa sổ này, mỗi lần cấp điện firmware
// "nhìn thấy" một cú chạm ảo: tắt chế độ đèn tự động, bật LED, và ghi một lần
// xuống NVS (đã đo trên Wokwi: saveSettings ở 2,173 s sau boot).
const unsigned long INPUT_SETTLE_MS     = 1500;
const unsigned long BREATHE_INTERVAL    = 30;
const unsigned long WINDMILL_INTERVAL   = 30;    // bước quét servo (activity 6)
const unsigned long FEED_DURATION       = 2000;  // thời gian mở cửa máng ăn (activity 10)
const unsigned long RESTART_DELAY_MS    = 200;   // chờ HTTP trả lời xong rồi mới restart
