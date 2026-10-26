#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// WiFi, ngưỡng điều khiển và mã bảo mật là các trường của SystemState
// (xem src/globals.h) — không khai báo biến rời ở đây nữa.

// ── Sơ đồ chân GPIO ──
#define DHT_PIN          23
#define DHT_TYPE         DHT11
#define RGB_PIN          19
#define RGB_COUNT        4
#define SERVO_PIN        13
#define BUZZER_PIN       25
#define RELAY_PIN        26
#define FAN_PIN          14
#define BLUE_LED_PIN     27
#define LED_PWM_CHANNEL  15      // Kênh LEDC cho PWM LED xanh dương (core 2.x); tránh xung đột timer với servo
#define TOUCH_PIN        39
#define PIR_PIN          33
#define LDR_PIN          34
#define SOIL_PIN         35
#define WATER_PIN        36
#define OLED_SDA         21
#define OLED_SCL         22
#define OLED_RESET       -1
#define OLED_I2C_ADDR    0x3C    // địa chỉ I2C chuẩn của module SSD1306 128x64
#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    64

// ── Thời gian (ms) ──
extern const unsigned long WIFI_CONNECT_TIMEOUT;
extern const unsigned long WIFI_CHECK_INTERVAL;
extern const unsigned long WIFI_STATUS_POLL_MS;   // chu kỳ hỏi trạng thái WiFi
extern const unsigned long SENSOR_INTERVAL;
extern const unsigned long OLED_INTERVAL;
extern const unsigned long OTA_HANDLE_INTERVAL;    // chu kỳ xử lý gói OTA
extern const unsigned long TOUCH_DEBOUNCE;
extern const unsigned long BREATHE_INTERVAL;
extern const unsigned long WINDMILL_INTERVAL;
extern const unsigned long FEED_DURATION;
extern const unsigned long RESTART_DELAY_MS;   // trễ trước khi ESP.restart() sau khi lưu WiFi

// ── I2C (OLED) ──
// Thời gian chờ tối đa cho một giao dịch I2C. Mặc định của thư viện Wire là
// 50 ms — quá dài: khi màn hình không cắm hoặc bus hỏng, MỖI giao dịch lỗi sẽ
// chặn loop() tới 50 ms. Hạ xuống 20 ms để loop không bị nghẽn.
constexpr uint16_t OLED_I2C_TIMEOUT_MS = 20;

// ── Web ──
// Kích thước buffer dựng JSON cho /api. JSON hiện khoảng 350 byte, để dư
// trên 150 byte để thêm trường mới. Bản build thử nghiệm đặt nhỏ hơn để kiểm
// tra nhánh "JSON bị cắt cụt" (firmware phải báo lỗi thay vì trả JSON hỏng).
#ifndef API_JSON_BUF_SIZE
#define API_JSON_BUF_SIZE 512
#endif

// ── Lọc cảm biến ──
constexpr uint8_t SENSOR_FILTER_SIZE = 5;  // Cửa sổ trung bình động

// ── Thích ứng phần cứng (hiệu chỉnh theo kit của bạn) ──
// Chiều đọc cảm biến theo tài liệu: LDR sáng = mức cao, đất khô = mức thấp,
// nước đầy = mức cao. Nếu module đọc ngược lại, hãy đặt cờ tương ứng
// thành 1 ở đây thay vì sửa bất kỳ logic điều khiển nào.
// (Mỗi cờ cũng có thể ghi đè từ dòng lệnh build, ví dụ -DINVERT_LIGHT=1)
#ifndef INVERT_LIGHT
#define INVERT_LIGHT     0
#endif
#ifndef INVERT_SOIL
#define INVERT_SOIL      0
#endif
#ifndef INVERT_WATER
#define INVERT_WATER     0
#endif

// Cực tính board relay bơm: 1 = module active-LOW (bơm chạy ở mức LOW)
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW 0
#endif

// Cực tính quạt: 1 = module active-LOW (quạt chạy ở mức LOW)
#ifndef FAN_ACTIVE_LOW
#define FAN_ACTIVE_LOW 0
#endif

// Bật/tắt còi cảnh báo: 0 = tắt còi hoàn toàn (im lặng), 1 = bật (mặc định)
#ifndef BUZZER_ENABLED
#define BUZZER_ENABLED 0
#endif

// Loại buzzer: 1 = buzzer thụ động (cần tone()), 0 = buzzer chủ động
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE   0
#endif
#ifndef BUZZER_TONE_HZ
#define BUZZER_TONE_HZ   2000
#endif

// Góc servo: hiệu chỉnh theo cơ cấu cơ khí (tránh kẹt bánh răng)
#ifndef WINDMILL_MIN_ANGLE
#define WINDMILL_MIN_ANGLE 0     // giới hạn dưới khi quét cánh quạt gió
#endif
#ifndef WINDMILL_MAX_ANGLE
#define WINDMILL_MAX_ANGLE 180   // giới hạn trên khi quét cánh quạt gió
#endif
#ifndef FEED_OPEN_ANGLE
#define FEED_OPEN_ANGLE    90    // vị trí mở cửa máng ăn
#endif
#ifndef FEED_CLOSE_ANGLE
#define FEED_CLOSE_ANGLE   0     // vị trí đóng cửa máng ăn
#endif

// ── Bảo vệ web ──
// Token tùy chọn cho các lệnh đổi trạng thái và /api (bật/tắt, chỉnh ngưỡng...).
// Để trống "" = không yêu cầu token (chỉ nên dùng trong mạng gia đình tin cậy).
// Token chỉ nằm ở firmware (compile-time), KHÔNG ghi plaintext vào NVS/flash.
// Đặt token rồi nạp lại firmware; mở web kèm token một lần để trang tự nhớ:
//   http://<ip-thiết-bị>/?token=MAT-KHAU-CUA-BAN
#ifndef WEB_TOKEN
#define WEB_TOKEN ""
#endif

// ── Mật khẩu OTA (cập nhật firmware từ xa) ──
// Chỉ nằm ở firmware (compile-time), không ghi vào NVS.
// Để trống "" = không yêu cầu password (chỉ nên dùng trong mạng gia đình tin cậy).
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

// ── Chống replay (1 = yêu cầu nonce cho mọi lệnh đổi trạng thái) ──
// Khi bật, client phải lấy nonce từ /api rồi gửi kèm mỗi lệnh. Lệnh cũ sẽ bị chặn.
#ifndef ANTI_REPLAY
#define ANTI_REPLAY 0
#endif


// ── Tiết kiệm điện (deep sleep) ──
// Thời gian ngủ sâu mặc định (giây) khi bật powerSave; có thể chỉnh từ web/API.
#ifndef DEFAULT_SLEEP_SECONDS
#define DEFAULT_SLEEP_SECONDS 30
#endif

#endif
