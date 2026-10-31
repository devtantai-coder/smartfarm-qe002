#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Thông tin WiFi/mật khẩu riêng nằm trong src/secrets.h (không commit).
// Sao chép src/secrets.h.example -> src/secrets.h rồi điền giá trị, hoặc
// truyền -DWIFI_SSID=... -DWIFI_PASSWORD=... khi build.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif

// WiFi, ngưỡng điều khiển và mã bảo mật là các trường của SystemState
// (xem src/globals.h) — không khai báo biến rời ở đây nữa.

// ── Sơ đồ chân GPIO ──
#define DHT_PIN          23
// Loại cảm biến DHT đi kèm. Mặc định DHT11 theo kit QE002; đổi bằng
// -DDHT_TYPE=DHT22 khi build (hoặc đổi trong src/secrets.h) nếu board dùng
// DHT22 — bộ mô phỏng Wokwi có part "wokwi-dht22", đọc bằng kiểu DHT11 sẽ trả
// giá trị sai (hoán đổi byte) chứ không báo lỗi.
#ifndef DHT_TYPE
#define DHT_TYPE         DHT11
#endif
#define RGB_PIN          19
#define RGB_COUNT        4
#define SERVO_PIN        13
#define BUZZER_PIN       25
#define RELAY_PIN        26
#define FAN_PIN          14
#define BLUE_LED_PIN     27
#define LED_PWM_CHANNEL  15      // Kênh LEDC cho PWM LED xanh dương (core 2.x); tránh xung đột timer với servo (xem ràng buộc ở FAN_PWM_CHANNEL)
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
extern const unsigned long SENSOR_INTERVAL;   // >= 2000 ms: nhịp DHT phục vụ (xem src/core/config.cpp)
extern const unsigned long OLED_INTERVAL;
extern const unsigned long OTA_HANDLE_INTERVAL;    // chu kỳ xử lý gói OTA
extern const unsigned long TOUCH_DEBOUNCE;
extern const unsigned long INPUT_SETTLE_MS;   // bỏ qua đầu vào số ngay sau khi cấp điện
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

// ETag của trang chủ. Tăng phiên bản khi ROOT_HTML thay đổi để trình duyệt
// tải lại trang mới thay vì nhận 304 với bản cache cũ.
#ifndef WEB_ROOT_ETAG
#define WEB_ROOT_ETAG "\"smartfarm-qe002-v1\""
#endif

// ── Lịch sử cảm biến + biểu đồ ──
// Ring buffer RAM: 96 mẫu, mỗi 30 giây một mẫu = khoảng 48 phút gần nhất.
// Không ghi flash liên tục để tránh mài mòn NVS; khởi động lại sẽ xóa lịch sử.
#ifndef HISTORY_ENABLED
#define HISTORY_ENABLED 1
#endif
#ifndef HISTORY_CAPACITY
#define HISTORY_CAPACITY 96
#endif
#ifndef HISTORY_INTERVAL_MS
#define HISTORY_INTERVAL_MS 30000UL
#endif
// Buffer mẫu để dựng JSON lịch sử (test dùng trực tiếp; handler /history cấp
// phát đúng historyJsonSize() byte, xem webserver.cpp). HISTORY_CAPACITY mẫu
// gọn nhất chiếm khoảng 7 KB.
#ifndef HISTORY_JSON_BUF_SIZE
#define HISTORY_JSON_BUF_SIZE 9216
#endif

// ── Thời tiết Internet (Open-Meteo, không cần API key) ──
// Tọa độ mặc định: Hà Nội. Đổi theo nơi lắp đặt (độ phân giải 4 chữ số thập phân).
#ifndef WEATHER_ENABLED
#define WEATHER_ENABLED 1
#endif
#ifndef WEATHER_LATITUDE
#define WEATHER_LATITUDE "21.0285"
#endif
#ifndef WEATHER_LONGITUDE
#define WEATHER_LONGITUDE "105.8542"
#endif
#ifndef WEATHER_REFRESH_MS
#define WEATHER_REFRESH_MS 900000UL   // 15 phút
#endif
#ifndef WEATHER_TIMEOUT_MS
#define WEATHER_TIMEOUT_MS 5000UL
#endif

// ── Thông báo Telegram (tùy chọn) ──
// Điền token/chat id trong src/secrets.h. Để trống = tắt hoàn toàn.
#ifndef TELEGRAM_BOT_TOKEN
#define TELEGRAM_BOT_TOKEN ""
#endif
#ifndef TELEGRAM_CHAT_ID
#define TELEGRAM_CHAT_ID ""
#endif
#ifndef TELEGRAM_MIN_INTERVAL_MS
#define TELEGRAM_MIN_INTERVAL_MS 30000UL
#endif
// Gửi hỏng (mất mạng, Telegram lỗi) không được "ăn" cả cửa sổ chống spam:
// sau khoảng này hệ thống được phép thử lại cùng loại sự kiện.
#ifndef TELEGRAM_RETRY_MS
#define TELEGRAM_RETRY_MS 5000UL
#endif
#ifndef TELEGRAM_TIMEOUT_MS
#define TELEGRAM_TIMEOUT_MS 8000UL
#endif
// PEM root CA để XÁC THỰC api.telegram.org. Để trống + TELEGRAM_TLS_INSECURE=1
// thì không xác thực chứng chỉ (tiện cho bản kit, KHÔNG an toàn).
// WiFiClientSecure kiểm tra chứng chỉ theo CA ghim sẵn, không cần đồng hồ đúng.
#ifndef TELEGRAM_CA_CERT
#define TELEGRAM_CA_CERT ""
#endif
#ifndef TELEGRAM_TLS_INSECURE
#define TELEGRAM_TLS_INSECURE 1
#endif

// ── Tác vụ mạng nền ──
// Mọi lời gọi mạng RA NGOÀI (Telegram, thời tiết) chạy trên một task riêng
// thay vì trong loop(): TLS/HTTP blocking trong loop() làm đứng web, OTA và
// logic điều khiển nhiều giây (nguy cơ watchdog 30 s).
#ifndef NETWORK_TASK_STACK
#define NETWORK_TASK_STACK 8192     // TLS cần stack lớn
#endif
#ifndef NETWORK_TASK_PRIORITY
#define NETWORK_TASK_PRIORITY 1     // ngang loopTask: nhường CPU cho web/control
#endif
#ifndef NETWORK_QUEUE_LEN
#define NETWORK_QUEUE_LEN 4
#endif

// ── ACEBOTT app (hoạt động 13) ──
// App QE002 gọi /config, /all, /door, /led, /rgb. Sau mỗi lệnh app, tự động
// điều khiển tạm dừng APP_CONTROL_PAUSE_MS để app không bị automation giành quyền.
#ifndef APP_CONTROL_PAUSE_MS
#define APP_CONTROL_PAUSE_MS 3000UL
#endif

// ── Cối xay gió: động cơ thật (HĐ6) ──
// Nối động cơ DC qua transistor/relay vào GPIO16. Đặt -1 nếu không dùng động cơ.
// Servo mô phỏng vẫn có thể chạy song song (WINDMILL_SERVO_SIM=1) như bản kit gốc.
#ifndef WINDMILL_MOTOR_PIN
#define WINDMILL_MOTOR_PIN 16
#endif
#ifndef WINDMILL_MOTOR_ACTIVE_HIGH
#define WINDMILL_MOTOR_ACTIVE_HIGH 1
#endif
#ifndef WINDMILL_SERVO_SIM
#define WINDMILL_SERVO_SIM 1
#endif

// ── PID điều tốc quạt ──
// 1 = dùng PWM trên FAN_PIN để điều tốc quạt theo PID nhiệt độ.
// 0 = giữ nguyên hành vi relay/bật-tắt (mặc định để tương thích kit).
#ifndef FAN_PWM_ENABLED
#define FAN_PWM_ENABLED 0
#endif
// Kênh LEDC cho quạt (CHỈ core 2.x dùng; core 3.x gắn kênh theo chân tự động).
// Ràng buộc core 2.x (esp32-hal-ledc.c): group = chan/8, timer = (chan/2)%4, và
// ledcWriteTone() của còi ép timer của kênh nó dùng về 10-bit theo tần số tone —
// hai kênh cùng timer mà khác độ phân giải/tần số thì kênh còn lại mất cấu hình.
// Kênh đã chiếm: 0 (servo, timer 0), 12 (tone còi, timer 2), 15 (LED xanh, timer 3).
// 13 từng bị dùng ở đây nhưng trùng timer 2 với còi -> quạt mất ~75% tốc độ mỗi lần
// báo động. 11 nằm ở timer 1 (nhóm HS), để dành 2/3/10 cho ESP32Servo cấp thêm.
#ifndef FAN_PWM_CHANNEL
#define FAN_PWM_CHANNEL 11       // kênh LEDC cho core 2.x (core 3 gắn theo chân)
#endif

// ── Khởi động mềm cho bơm ──
// Trễ ngắn trước khi đóng relay bơm ở chế độ tự động (giảm sốc dòng/áp).
// 0 = đóng ngay (tương thích hành vi cũ).
#ifndef SOFTSTART_MS
#define SOFTSTART_MS 500UL
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
#define BUZZER_ENABLED 1
#endif

// Loại buzzer: 1 = buzzer thụ động (cần tone()), 0 = buzzer chủ động.
// Module P-Buzzer của kit QE002 là loại THỤ ĐỘNG (cần xung 2–5 kHz mới kêu)
// và part "wokwi-buzzer" cũng là buzzer thụ động, nên mặc định là 1.
// Đổi thành 0 nếu bạn thay bằng buzzer chủ động (chỉ cần cấp mức HIGH).
#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE   1
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
// Để trống "" = TẮT OTA (không gọi ArduinoOTA.begin()).
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

// ── Chống replay (1 = yêu cầu nonce cho mọi lệnh đổi trạng thái) ──
// Khi bật, client phải lấy nonce từ /api rồi gửi kèm mỗi lệnh. Lệnh cũ sẽ bị chặn.
#ifndef ANTI_REPLAY
#define ANTI_REPLAY 0
#endif


// ── Tiết kiệm điện (deep sleep) ──
// Thời gian ngủ sâu mặc định (giây) khi bật powerSave. Đây là giá trị
// compile-time; giá trị đã lưu trong NVS (loadSettings) sẽ thay thế nó.
// Hiện chưa có route web/API đổi powerSave/sleepSeconds — phải sửa trong mã
// nguồn và nạp lại firmware.
#ifndef DEFAULT_SLEEP_SECONDS
#define DEFAULT_SLEEP_SECONDS 30
#endif
// Cửa sổ thức trước khi ngủ (ms). Mỗi lần tỉnh dậy firmware chạy lại setup()
// (mất lịch sử RAM, đọc lại NVS, attach lại servo), nên cửa sổ quá ngắn làm
// dashboard/OTA gần như không dùng được khi bật powerSave.
#ifndef POWER_SAVE_AWAKE_MS
#define POWER_SAVE_AWAKE_MS 30000UL
#endif

// Modem-sleep của WiFi: 0 = tắt (web phản hồi nhanh nhất, mặc định hiện tại),
// 1 = bật (giảm dòng tiêu thụ, đổi lại thêm tối đa ~1 chu kỳ beacon độ trễ).
#ifndef WIFI_MODEM_SLEEP
#define WIFI_MODEM_SLEEP 0
#endif

// Tần số CPU (MHz). 240 = mặc định; 160 hoặc 80 giảm điện nhưng làm chậm
// tính toán/TLS (worker mạng) — đo lại nếu đổi.
#ifndef CPU_FREQ_MHZ
#define CPU_FREQ_MHZ 240
#endif

// Bluetooth serial (điều khiển từ xa qua điện thoại): 1 = bật, 0 = tắt (mặc định).
// LÝ DO MẶC ĐỊNH TẮT — hai lựa chọn này loại trừ nhau trên flash 4 MB của kit:
//   - Bật Bluetooth: firmware 1.936.079 byte, KHÔNG vừa partition mặc định
//     (1.310.720 byte) → buộc phải dùng PartitionScheme=huge_app, mà huge_app
//     chỉ có MỘT slot app (không có ot_1) nên esp_ota_get_next_update_partition()
//     trả NULL: OTA chết, và Wokwi cũng không nạp được ảnh (boot loop).
//   - Tắt Bluetooth: firmware 1.221.295 byte (93% partition mặc định) → giữ được
//     OTA với hai slot app và chạy được trong mô phỏng Wokwi.
// Muốn dùng Bluetooth thì đặt -DBLUETOOTH_ENABLED=1 và PartitionScheme=huge_app,
// đồng thời chấp nhận mất OTA.
#ifndef BLUETOOTH_ENABLED
#define BLUETOOTH_ENABLED 0
#endif

// ── Kết nối chuyên gia ──
// MQTT cần thư viện PubSubClient (xem README) khi bật = 1.
#ifndef MQTT_ENABLED
#define MQTT_ENABLED 0
#endif

#ifndef MQTT_BROKER
#define MQTT_BROKER ""
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_TOPIC_PREFIX
#define MQTT_TOPIC_PREFIX "smartfarm"
#endif
#ifndef MQTT_USE_TLS
#define MQTT_USE_TLS 0        // 1 = broker qua TLS (cần MQTT_CA_CERT hoặc chủ định Tin cậy)
#endif
#ifndef MQTT_TLS_INSECURE
#define MQTT_TLS_INSECURE 0   // 0 = BẮT BUỘC xác thực chứng chỉ (cần MQTT_CA_CERT)
#endif
#ifndef MQTT_SOCKET_TIMEOUT_S
#define MQTT_SOCKET_TIMEOUT_S 2   // giây; mặc định thư viện 15 s làm nghẽn loop()
#endif
// Chặn bắt tay TLS khi broker không trả lời. Core mặc định có thể chờ tới
// 120 s trên loopTask — watchdog 30 s sẽ panic reboot giữa chừng.
#ifndef MQTT_HANDSHAKE_TIMEOUT_S
#define MQTT_HANDSHAKE_TIMEOUT_S 10
#endif
#ifndef MQTT_RETRY_MIN_MS
#define MQTT_RETRY_MIN_MS 5000UL
#endif
#ifndef MQTT_RETRY_MAX_MS
#define MQTT_RETRY_MAX_MS 60000UL
#endif
#ifndef MQTT_CA_CERT
#define MQTT_CA_CERT ""       // PEM root CA; để trống + INSECURE=0 sẽ không kết nối
#endif
#ifndef MQTT_PUBLISH_INTERVAL_MS
#define MQTT_PUBLISH_INTERVAL_MS 10000UL
#endif

#endif  // CONFIG_H
