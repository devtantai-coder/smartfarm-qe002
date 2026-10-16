# Smart Farm QE002 ACEBOTT - Hướng dẫn sử dụng chi tiết

Dự án vườn nông trồng thông minh (IoT Smart Farm) sử dụng mạch ESP32, viết bằng ngôn ngữ C++ trên Arduino IDE. Giao diện web và toàn bộ tài liệu hướng dẫn bằng **tiếng Việt có dấu**, dễ hiểu ngay cả với người chưa biết lập trình (chú thích trong code viết ngắn gọn bằng tiếng Anh).

Nội dung hướng dẫn này được phát triển dựa trên **Tài liệu chính thức bộ kit IoT Smart Farm** và **Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM** của ACEBOTT.

---

## 1. Dự án làm gì?

Bộ mạch sẽ giúp bạn quản lý một vườn trồng nhỏ tự động:

- **Giám sát môi trường:** Đo nhiệt độ, độ ẩm không khí, ánh sáng, độ ẩm đất, mức nước trong thùng.
- **Phát hiện sự kiện:** Cảm biến chuyển động (PIR), nút chạm cảm ứng.
- **Điều khiển tự động:** Bật máy bơm khi đất khô, tắt khi đất ẩm; bật đèn khi tối, tắt khi sáng; đổi màu đèn RGB theo ánh sáng.
- **Báo động:** Kích hoạt còi và đèn xanh khi có chuyển động hoặc nước cạn.
- **Điều khiển từ xa:** Truy cập qua trình duyệt trên điện thoại/máy tính qua WiFi để bật/tắt thiết bị, xem số liệu thời gian thực.
- **Lưu cài đặt:** Các chế độ tự động và ngưỡng cảnh báo được lưu vào bộ nhớ flash ESP32, không mất khi mất điện.

---

## 2. Mapping chân (Pinout) - Từ tài liệu chính thức

Dựa trên tài liệu "Bước 19: Sơ đồ đấu dây" và "Bước 20: Hướng dẫn đấu dây":

| Chân ESP32 | Thiết bị | Ghi chú từ tài liệu |
|------------|----------|---------------------|
| GPIO 23 | Cảm biến DHT11 | Nhiệt độ và độ ẩm không khí |
| GPIO 19 | Dây đèn LED RGB | 4 đèn RGB, điều khiển màu |
| GPIO 13 | Động cơ servo SG90 9G | Mở/đóng cửa nhà kính, điều chỉnh mái che, van nước |
| GPIO 25 | Mạch còi báo (P-Buzzer) | Báo động khi có sự cố |
| GPIO 26 | Mạch rơ-le 5V | Điều khiển máy bơm nước (quắt qua Relay) |
| GPIO 27 | Mạch đèn LED xanh dương | Đèn báo trạng thái hệ thống |
| I2C (SDA/SCL) | Mạch OLED | SDA=GPIO21, SCL=GPIO22 |
| GPIO 39 | Cảm biến chạm | Bật/tắt thiết bị bằng tay |
| GPIO 33 | Cảm biến chuyển động PIR | Phát hiện người/động vật |
| GPIO 34 | Cảm biến ánh sáng (LDR) | Đo cường độ ánh sáng |
| GPIO 35 | Cảm biến độ ẩm đất | Đo độ ẩm đất |
| GPIO 36 | Cảm biến nước | Kiểm tra mực nước trong bể |

**Lưu ý quan trọng về chân:**
- GPIO 34, 35, 36 là kênh ADC1, an toàn dùng cùng lúc với WiFi.
- GPIO 39 là input-only, không có pull-up nội bộ. Nút chạm cần điện trở pull-down phần cứng.
- Servo SG90: dây đỏ→V, dây nâu→G, dây vàng→chân số 13.
- GPIO 14 (quạt) và GPIO 12 (đèn thở PWM) là các chân **mở rộng** của dự án, không có trong sơ đồ gốc của kit.

---

## 3. Danh sách thiết bị và chức năng (từ tài liệu)

| Thành phần | Chức năng chính |
|------------|-----------------|
| ESP32 (Bộ điều khiển trung tâm) | Vi não hệ thống, nhận dữ liệu cảm biến, điều khiển thiết bị, kết nối WiFi/Bluetooth |
| DHT11 | Đo nhiệt độ (0-50°C, sai số ±2°C), độ ẩm không khí (20-80%, sai số ±5%) |
| Module Relay 5V | Công tắc điện tử điều khiển thiết bị công suất lớn (máy bơm, quạt, đèn) |
| Cảm biến ánh sáng (LDR) | Đo cường độ ánh sáng, khi thiếu → hệ thống bật đèn LED cho cây |
| Máy bơm nước | Tưới nước cho cây, bật/tắt tự động hoặc qua điện thoại |
| RGB LED Strip | Hiển thị trạng thái hệ thống, tạo ánh sáng bổ sung cho cây |
| Servo SG90 | Mở/đóng cửa nhà kính, điều chỉnh mái che, điều khiển van nước |
| P-Buzzer Module | Báo động khi có sự cố, thông báo cảm biến vượt ngưỡng |
| Blue LED Module | Đèn LED màu xanh, báo trạng thái hệ thống |
| OLED Module | Hiển thị nhiệt độ, độ ẩm, độ ẩm đất, trạng thái thiết bị |
| Touch Sensor | Cảm biến chạm để điều khiển hệ thống |
| PIR Motion Sensor | Phát hiện người/động vật, kích hoạt cảnh báo hoặc bật đèn |
| Soil Moisture Sensor | Đo độ ẩm đất, khi đất khô → bật bơm nước tự động |
| Water Sensor | Kiểm tra mực nước trong bể, tránh máy bơm chạy khi hết nước |
| Hộp đựng pin (6xAA) | Cấp điện cho mô hình |
| Solar Panel | Tích hợp năng lượng mặt trời (tùy chọn, thân thiện môi trường) |

---

## 4. Cấu trúc thư mục

```
duan123/
├── SmartFarm.ino          # File chính (setup và loop - điểm bắt đầu)
├── README.md              # File hướng dẫn này
├── src/
│   ├── config.h / config.cpp        # Cấu hình: WiFi, chân, ngưỡng, thời gian
│   ├── globals.h / globals.cpp      # Biến toàn cục và đối tượng phần cứng
│   ├── sensors.h / sensors.cpp      # Hàm đọc cảm biến
│   ├── actuators.h / actuators.cpp  # Hàm điều khiển thiết bị (relay, đèn, RGB, servo)
│   ├── settings.h / settings.cpp    # Lưu/tải cài đặt vào bộ nhớ flash ESP32
│   ├── oled.h / oled.cpp            # Hiển thị màn hình OLED
│   ├── webserver.h / webserver.cpp  # Web server và giao diện điều khiển từ điện thoại
│   ├── wifi.h / wifi.cpp            # Quản lý kết nối và kết nối lại WiFi
│   ├── logger.h / logger.cpp        # Hệ thống log có mốc thời gian, mã lỗi, debug menu
│   └── control.h / control.cpp      # Logic tự động (bơm, đèn, RGB, chạm, báo động)
├── Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM.pdf
└── Tài liệu.pdf
```

---

## 5. Đối chiếu hoạt động STEAM với chức năng code

Dựa trên **Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM**:

| Hoạt động STEAM | Tên hoạt động | Chức năng trong code | File code liên quan |
|-----------------|---------------|----------------------|---------------------|
| Hoạt động 1 | Xây dựng lại nông trại | Lắp ráp phần cứng | - |
| Hoạt động 2 | Thắp sáng nông trại | Bật đèn LED xanh (GPIO 27) | actuators.cpp |
| Hoạt động 3 | Điều khiển LED | Hiệu ứng thở (PWM) | - (có thể mở rộng) |
| Hoạt động 4 | Điều khiển ánh sáng LED | Nút chạm bật/tắt đèn | control.cpp (controlTouch) |
| Hoạt động 5 | Báo động động vật | Cảm biến PIR báo động | control.cpp (controlAlerts) |
| Hoạt động 6 | Cối xoay gió năng lượng mặt trời | - (mở rộng sau) | - |
| Hoạt động 7 | Phát hiện độ ẩm đất, Tưới nước tự động | Cảm biến đất + bơm tự động | sensors.cpp, control.cpp (controlPump) |
| Hoạt động 8 | Phát hiện mực nước | Cảm biến nước + còi báo | sensors.cpp, control.cpp (controlAlerts) |
| Hoạt động 9 | Đèn tăng trưởng thực vật | Đèn RGB theo ánh sáng | control.cpp (controlRgb) |
| Hoạt động 10 | Cho ăn tự động | - (mở rộng sau) | - |
| Hoạt động 11 | Màn hình OLED hiển thị thời tiết | OLED hiển thị T, H, ánh sáng | oled.cpp |
| Hoạt động 12 | Internet điều khiển vạn vật từ xa | WebServer + API | webserver.cpp |
| Hoạt động 13 | Sử dụng smart phone để điều khiển nông trại | Giao diện web trên điện thoại | webserver.cpp |

---

## 6. Quy tắc an toàn kỹ thuật (từ tài liệu)

1. **An toàn về nguồn điện:**
   - Sử dụng nguồn điện đúng điện áp theo yêu cầu của các thiết bị (ESP32, relay, máy bơm, cảm biến).
   - Kiểm tra dây điện và các mối nối trước khi vận hành để tránh chập điện.
   - Không chạm vào dây điện hoặc thiết bị khi tay ướt.

2. **Bảo vệ linh kiện điện tử:**
   - Tránh để nước tiếp xúc trực tiếp với các linh kiện điện tử như ESP32, relay hoặc mạch điện.
   - Lắp đặt các linh kiện cố định và chắc chắn để tránh rơi hoặc hư hỏng.

3. **An toàn khi sử dụng máy bơm nước:**
   - Kiểm tra máy bơm và ống dẫn nước trước khi sử dụng.
   - **Không để máy bơm chạy khi không có nước**, vì có thể làm hỏng thiết bị. (Code đã tích hợp kiểm tra này trong `controlPump()`)
   - Đảm bảo hệ thống tưới nước không bị rò rỉ.

4. **An toàn khi lập trình và kết nối thiết bị:**
   - Kết nối ESP32 với máy tính bằng cáp USB đúng chuẩn khi nạp chương trình.
   - Không rút dây hoặc tháo linh kiện khi hệ thống đang hoạt động.

5. **Kiểm tra và bảo trì định kỳ:**
   - Thường xuyên kiểm tra cảm biến và các thiết bị để đảm bảo hoạt động chính xác.

---

## 7. Cài đặt Arduino IDE trên Linux

### Cách 1: Qua Snap (Ubuntu/Debian khuyên dùng)
```bash
sudo snap install arduino
```

### Cách 2: Tải trực tiếp từ trang chủ
1. Truy cập: https://www.arduino.cc/en/software
2. Tải file `.tar.xz` cho Linux
3. Giải nén và chạy file `arduino-ide`

### Cấp quyền truy cập cổng USB
```bash
sudo usermod -a -G dialout $USER
```
Sau đó **đăng xuất và đăng nhập lại** máy để quyền có hiệu lực.

---

## 8. Cài đặt board ESP32 và thư viện

### Bước 1: Thêm board ESP32 vào Arduino IDE
1. Mở Arduino IDE → **File → Preferences**
2. Ở mục "Additional Boards Manager URLs" dán:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Vào **Tools → Board → Boards Manager**, tìm `esp32`, cài đặt **esp32 by Espressif Systems**

> **Tương thích:** Code tự động nhận biết phiên bản Arduino-ESP32 core 2.x hoặc 3.x (watchdog và PWM LEDC được khởi tạo theo đúng API của từng core), nên bạn có thể dùng bản core mới nhất.

### Bước 2: Cài đặt thư viện
Vào **Sketch → Include Library → Manage Libraries**, cài đặt:
- DHT sensor library (Adafruit)
- Adafruit SSD1306
- Adafruit NeoPixel
- Adafruit GFX Library
- ESP32 Servo

---

## 9. Chuẩn bị code trước khi nạp

### Bước 1: Mở dự án
Mở Arduino IDE, chọn **File → Open**, mở file `SmartFarm.ino` trong thư mục `duan123`.

### Bước 2: Cấu hình WiFi
Mở file `src/config.cpp`, sửa:

```cpp
const char *ssid = "TEN_WIFI_CUA_BAN";
const char *password = "MAT_KHAU_WIFI_CUA_BAN";
```

### Bước 3: Chọn board và cổng
- **Tools → Board:** ESP32 Dev Module
- **Tools → Port:** Cổng USB của ESP32 (ví dụ: `/dev/ttyUSB0`)

### Bước 4: Nạp code
Nhấn nút Upload (➡) hoặc **Sketch → Upload**.

---

## 10. Sử dụng dự án

### Bước 1: Xem địa chỉ IP
1. Mở **Tools → Serial Monitor**, 115200 baud.
2. Nhấn nút RESET trên ESP32.
3. Đợi thấy: `Địa chỉ IP: 192.168.1.xxx`

### Bước 2: Truy cập giao diện web
Trên điện thoại cùng mạng WiFi, mở trình duyệt, gõ địa chỉ IP. Trang web hiển thị:
- **Cảm biến thực tế:** Nhiệt độ, độ ẩm không khí, ánh sáng, độ ẩm đất, mức nước, chuyển động, chạm.
- **Điều khiển thiết bị:** Máy bơm, đèn xanh, màu RGB, servo (cửa).
- **Chế độ tự động:** Bật/tắt tự động cho bơm, đèn, RGB, quạt, đèn thở.

> **Lưu ý:** Khi bạn nhấn nút **Bật/Tắt** trên web (hoặc dùng cảm biến chạm), chế độ **tự động** của thiết bị đó sẽ tự tắt để tránh việc hệ thống tự động bật/tắt "giằng co" với lệnh thủ công của bạn. Muốn quay lại tự động, hãy nhấn nút **Tự động** tương ứng.

### Bước 3: Theo dõi OLED
Màn hình OLED hiển thị: Nhiệt độ, độ ẩm, ánh sáng, độ ẩm đất, mức nước, trạng thái máy bơm.

---

## 11. Các tính năng an toàn đã tích hợp trong code

| Tính năng | Mô tả | Vị trí code |
|-----------|-------|-------------|
| Kiểm tra nước trước khi bơm | Không bật máy bơm nếu nước cạn (bảo vệ bơm) | `control.cpp: controlPump()` |
| Hysterese độ ẩm đất | Tránh bơm nhấp nháy khi đất dao động quanh ngưỡng | `control.cpp: controlPump()` |
| Chống dội + phát hiện sườn nút chạm | Mỗi lần chạm chỉ đảo bơm đúng 1 lần và tự chuyển sang thủ công | `control.cpp: controlTouch()` |
| Điều khiển thủ công tắt tự động | Bấm Bật/Tắt trên web sẽ tự tắt chế độ tự động của thiết bị đó | `webserver.cpp: handleToggle()` |
| Còi báo theo trạng thái | Nước cạn kêu liên tục, chuyển động kêu ngắt quãng; chỉ ghi log 1 lần mỗi sự kiện (không spam Serial) | `control.cpp: controlAlerts()` |
| Bộ đệm trung bình động 5 mẫu | Lọc nhiễu cho toàn bộ cảm biến | `sensors.cpp` |
| Tự kết nối lại WiFi | Thử lại ở mọi trạng thái mất mạng, không lặp log gây nhiễu | `wifi.cpp: manageWiFi()` |
| Lưu cài đặt vào flash | Không mất cài đặt khi mất điện | `settings.cpp` |
| Kiểm tra lỗi DHT11 | Giữ giá trị cũ khi đọc lỗi, đếm số lần lỗi để báo mã 501 | `sensors.cpp: readAllSensors()` |
| Giới hạn giá trị từ web | Chặn servo 0–180°, màu RGB 0–255, ngưỡng trong khoảng hợp lệ | `webserver.cpp` |
| Watchdog an toàn | Nạp watchdog trong lúc chờ WiFi, tự khởi động lại nếu chương trình treo | `SmartFarm.ino` |

---

## 12. Hệ thống Logger và Mã Lỗi

Dự án sử dụng hệ thống logger riêng (`src/logger.h`, `src/logger.cpp`) để ghi nhật ký hoạt động ra Serial Monitor. Điều này giúp bạn dễ dàng xác định lỗi khi hệ thống không hoạt động như mong đợi.

### Cấp độ log

| Cấp độ | Màu sắc | Ý nghĩa |
|--------|---------|---------|
| DEBUG  | Xanh dương nhạt | Thông tin chi tiết cho lập trình viên |
| INFO   | Xanh lá | Hoạt động bình thường của hệ thống |
| WARNING| Vàng | Cảnh báo có vấn đề nhưng hệ thống vẫn chạy |
| ERROR  | Đỏ | Lỗi nghiêm trọng, một chức năng không hoạt động |
| FATAL  | Tím | Lỗi chết hệ thống, phải khởi động lại |

### Định dạng log

```
[00:00:05.123] [INFO ] setup:132 -> Settings loaded
[00:00:06.456] [ERROR] setup:106 [MA LOI: 101] -> OLED not found
```

Thông tin trong mỗi dòng log:
- **Thời gian:** Thời gian chạy từ khi ESP32 bật (giờ:phút:giây.miligiây)
- **Cấp độ:** Mức độ nghiêm trọng của thông tin (đệm 5 ký tự cho thẳng cột)
- **Hàm:Số dòng:** Tên hàm và số dòng trong file source (dễ tìm vào code)
- **Nội dung:** Thông báo chi tiết (tiếng Anh ngắn gọn)

### Bảng mã lỗi

| Mã lỗi | Ý nghĩa | Cách khắc phục |
|--------|---------|----------------|
| 101 | OLED không phát hiện được | Kiểm tra dây SDA (GPIO21), SCL (GPIO22) và địa chỉ I2C (0x3C) |
| 102 | Không thể kết nối WiFi qua 30 giây | Kiểm tra tên/mật khẩu WiFi trong `src/config.cpp`, tín hiệu WiFi; hệ thống vẫn tự thử lại nền |
| 201 | Nước cạn, tắt máy bơm để bảo vệ thiết bị | Thêm nước vào thùng, kiểm tra cảm biến mức nước (GPIO 36) |
| 202 | Đất khô, bật máy bơm tưới | Thông báo bình thường, không cần can thiệp |
| 203 | Đất đủ ẩm, tắt máy bơm | Thông báo bình thường, không cần can thiệp |
| 301 | Có chuyển động phát hiện | Thông báo bình thường từ cảm biến PIR (GPIO 33) |
| 302 | Nước cạn, kéo còi báo động liên tục | Thêm nước vào thùng ngay |
| 401 | Mất WiFi, đang kết nối lại | Kiểm tra modem/tín hiệu; hệ thống tự thử lại mỗi 10 giây (chỉ ghi log 1 lần mỗi lần mất mạng) |
| 501 | Cảm biến DHT11 lỗi liên tục | Kiểm tra dây tín hiệu GPIO 23, nguồn 3.3V của DHT11 hoặc thay cảm biến |

### Cách sử dụng logger trong code

```cpp
LOG_INFO("Pump: ON");                             // Thông tin bình thường
LOG_WARN("WiFi signal weak");                     // Cảnh báo
LOG_ERROR("Sensor timeout");                      // Lỗi
LOG_ERROR_CODE(201, "Low water, pump OFF");       // Lỗi kèm mã
LOG_DEBUG("Extra detail only at DEBUG level");    // Chỉ hiển thị nếu cấp độ >= DEBUG
```

Để thay đổi cấp độ log tối thiểu hiển thị, sửa biến `gMinLogLevel` trong `src/logger.cpp`:
- `LOG_DEBUG`: Hiển thị tất cả (rất chi tiết)
- `LOG_INFO`: Hiển thị INFO, WARN, ERROR, FATAL (mặc định)
- `LOG_WARNING`: Chỉ hiển thị WARN, ERROR, FATAL
- `LOG_ERROR`: Chỉ hiển thị ERROR, FATAL
- `LOG_FATAL`: Chỉ hiển thị FATAL

---

## 13. Khắc phục sự cố thường gặp

| Vấn đề | Nguyên nhân | Cách khắc phục |
|--------|-------------|----------------|
| Không nạp được code | Sai board/cổng USB | Kiểm tra **Tools → Board** và **Tools → Port** |
| Không thấy địa chỉ IP | Sai WiFi hoặc tín hiệu yếu | Sửa `src/config.cpp`, đặt ESP32 gần modem |
| OLED không hiển thị | Sai dây SDA/SCL hoặc địa chỉ I2C | Kiểm tra SDA→21, SCL→22, địa chỉ 0x3C |
| Máy bơm không bật | Nước cạn hoặc sai chân relay | Kiểm tra nước trong bể, chân relay 26 |
| Còi kêu liên tục | Nước cạn hoặc có chuyển động | Thêm nước hoặc kiểm tra PIR |
| Cảm biến đọc sai | Dây lỏng hoặc cảm biến hỏng | Kiểm tra dây nối, thử cảm biến khác |

---

## 14. Mở rộng nâng cao (từ tài liệu)

- Tích hợp AI dự đoán bệnh cây
- Hệ thống cảnh báo qua điện thoại (push notification)
- Điều khiển từ xa bằng app di động
- Ứng dụng năng lượng mặt trời (tấm pin mặt trời đã có trong kit)
- Thêm quạt điều khiển qua relay (relay có thể điều khiển bơm, quạt, đèn)

---

## 15. Tác giả và bản quyền

Dự án được xây dựng cho bộ starter kit **QE002 ACEBOTT IOT Smart Farm**. Code được viết và tối ưu hóa để chạy trên Arduino IDE với mạch ESP32, toàn bộ tài liệu bằng tiếng Việt có dấu. Tham khảo từ tài liệu chính thức của ACEBOTT.

Chúc bạn thành công với vườn nông trồng thông minh! 🌱