#include "config.h"

// Các giá trị cấu hình có thể thay đổi lúc chạy (WiFi, ngưỡng, mã bảo mật)
// nằm trong SystemState — xem src/globals.h / globals.cpp.

// ── Thời gian ──
const unsigned long WIFI_CONNECT_TIMEOUT = 30000;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long SENSOR_INTERVAL     = 1000;
const unsigned long OLED_INTERVAL       = 2000;
const unsigned long TOUCH_DEBOUNCE      = 300;
const unsigned long BREATHE_INTERVAL    = 30;
const unsigned long WINDMILL_INTERVAL   = 30;    // bước quét servo (activity 6)
const unsigned long FEED_DURATION       = 2000;  // thời gian mở cửa máng ăn (activity 10)
const unsigned long RESTART_DELAY_MS    = 200;   // chờ HTTP trả lời xong rồi mới restart
