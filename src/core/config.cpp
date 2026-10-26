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
const unsigned long SENSOR_INTERVAL     = 1000;
const unsigned long OLED_INTERVAL       = 2000;
// ArduinoOTA.handle() đọc một gói UDP mỗi lần gọi. OTA không nhạy cảm với độ
// trễ nên gọi mỗi 20 ms là quá đủ, thay vì mỗi vòng loop (~1 kHz).
const unsigned long OTA_HANDLE_INTERVAL  = 20;
const unsigned long TOUCH_DEBOUNCE      = 300;
const unsigned long BREATHE_INTERVAL    = 30;
const unsigned long WINDMILL_INTERVAL   = 30;    // bước quét servo (activity 6)
const unsigned long FEED_DURATION       = 2000;  // thời gian mở cửa máng ăn (activity 10)
const unsigned long RESTART_DELAY_MS    = 200;   // chờ HTTP trả lời xong rồi mới restart
