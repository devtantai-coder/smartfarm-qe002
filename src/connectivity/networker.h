#ifndef SMARTFARM_NETWORKER_H
#define SMARTFARM_NETWORKER_H

#include <Arduino.h>

// ── Tác vụ mạng nền ──
// Toàn bộ I/O mạng RA NGOÀI (Telegram, thời tiết Open-Meteo) chạy trên MỘT task
// riêng thay vì trong loop(). Lý do: client.connect() + đọc phản hồi là blocking,
// một lần gửi Telegram có thể giữ CPU 10 s — đủ để web server, OTA và logic
// điều khiển đứng hình, thậm chí vấp watchdog 30 s.
//
// Từ loop() chỉ còn thao tác "đẩy job vào queue" (không chặn, không cấp phát).
void netWorkerInit();

// Xếp một thông báo vào queue. Trả false khi hàng đợi đầy hoặc task chưa chạy.
bool netWorkerPostNotify(const char* kind, const char* message);

// Yêu cầu cập nhật thời tiết. Nhiều yêu cầu liên tiếp được gộp thành một.
bool netWorkerPostWeather();

#ifdef SMARTFARM_TEST_HOOKS
void testResetNetWorker();
#endif

#endif  // SMARTFARM_NETWORKER_H
