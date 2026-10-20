# Smart Farm QE002 ACEBOTT - Hướng dẫn sử dụng chi tiết

Dự án vườn nông trồng thông minh (IoT Smart Farm) sử dụng mạch ESP32, viết bằng ngôn ngữ C++ trên Arduino IDE. Giao diện web và toàn bộ tài liệu hướng dẫn bằng **tiếng Việt có dấu**, dễ hiểu ngay cả với người chưa biết lập trình (chú thích trong code viết ngắn gọn bằng tiếng Anh).

Nội dung hướng dẫn này được phát triển dựa trên **Tài liệu chính thức bộ kit IoT Smart Farm** và **Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM** của ACEBOTT.

---

## 1. Dự án làm gì?

Bộ mạch sẽ giúp bạn quản lý một vườn trồng nhỏ tự động:

- **Giám sát môi trường:** Đo nhiệt độ, độ ẩm không khí, ánh sáng, độ ẩm đất, mức nước trong thùng.
- **Phát hiện sự kiện:** Cảm biến chuyển động (PIR), nút chạm cảm ứng.
- **Điều khiển tự động:** Bật máy bơm khi đất khô, tắt khi đất ẩm; bật đèn khi tối, tắt khi sáng; đèn RGB tăng trưởng (LED 1&3 đỏ, 2&4 xanh dương) khi tối.
- **Đèn thông minh:** Nút chạm bật/tắt đèn LED xanh, hiệu ứng "đèn thở" PWM, PIR tự bật đèn khi có người và tắt khi người rời đi.
- **Báo động:** Còi kêu liên tục khi nước cạn.
- **Điều khiển từ xa:** Truy cập qua trình duyệt trên điện thoại/máy tính qua WiFi để bật/tắt thiết bị, xem số liệu thời gian thực (kể cả lệnh `/on`, `/off`).
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
- Đèn LED xanh (GPIO 27) được điều khiển bằng PWM nên dùng chung cho cả 3 chế độ: bật/tắt (chạm, web, PIR), hiệu ứng đèn thở và web `/on` `/off`. Ba chế độ tự loại trừ nhau.
- GPIO 14 (quạt) là chân **mở rộng** của dự án, không có trong sơ đồ gốc của kit.

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
| Blue LED Module | Đèn LED màu xanh: báo trạng thái, đèn thở PWM, bật/tắt bằng chạm/PIR/web |
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
│   ├── config.h / config.cpp        # Hằng số cấu hình: chân GPIO, thời gian, cờ phần cứng
│   ├── globals.h / globals.cpp      # SystemState (một struct chứa toàn bộ state) + đối tượng phần cứng
│   ├── sensors.h / sensors.cpp      # Hàm đọc cảm biến
│   ├── actuators.h / actuators.cpp  # Hàm điều khiển thiết bị (relay, đèn, RGB, servo)
│   ├── settings.h / settings.cpp    # Lưu/tải cài đặt vào bộ nhớ flash ESP32
│   ├── oled.h / oled.cpp            # Hiển thị màn hình OLED
│   ├── webserver.h / webserver.cpp  # Web server và giao diện điều khiển từ điện thoại
│   ├── wifi.h / wifi.cpp            # Quản lý kết nối và kết nối lại WiFi
│   ├── logger.h / logger.cpp        # Hệ thống log có mốc thời gian, mã lỗi, debug menu
│   ├── demo.h / demo.cpp            # Chế độ trình diễn lần lượt các hoạt động (HĐ2–HĐ12)
│   └── control.h / control.cpp      # Logic tự động (bơm, đèn, RGB, chạm, báo động, servo)
├── Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM.pdf
└── Tài liệu.pdf
```

---

## 5. Đối chiếu hoạt động STEAM với chức năng code

Dựa trên **Danh mục tổ chức hoạt động trải nghiệm STEM_STEAM**:

| Hoạt động STEAM | Tên hoạt động | Chức năng trong code | File code liên quan |
|-----------------|---------------|----------------------|---------------------|
| Hoạt động 1 | Xây dựng lại nông trại | Lắp ráp phần cứng | - |
| Hoạt động 2 | Thắp sáng nông trại | Bật đèn LED xanh (GPIO 27): qua web, `/on`, nút chạm hoặc tự động khi tối | actuators.cpp (setBlueLed), control.cpp (controlLight) |
| Hoạt động 3 | Điều khiển LED | Hiệu ứng đèn thở PWM trên đèn LED xanh GPIO 27 | control.cpp (controlBreathe), actuators.cpp (setLedPwm) |
| Hoạt động 4 | Điều khiển ánh sáng LED | Nút chạm bật/tắt đèn LED xanh (nhấn lần đầu bật, nhấn lần nữa tắt) | control.cpp (controlTouch) |
| Hoạt động 5 | Báo động động vật | PIR: có người thì bật đèn, người rời đi thì tắt đèn | control.cpp (controlAlerts) |
| Hoạt động 6 | Cối xoay gió năng lượng mặt trời | Mô hình gốc quay bằng động cơ nối trực tiếp pin mặt trời; code mô phỏng thêm bằng servo GPIO13 (quay qua lại 0–180°) với nút "Cối xay gió" trên web và endpoint `/windmill` | control.cpp (setWindmill/controlWindmill), webserver.cpp (handleWindmill) |
| Hoạt động 7 | Phát hiện độ ẩm đất, Tưới nước tự động | Cảm biến đất + bơm tự động; dữ liệu in ra Serial mỗi giây ở mức log DEBUG | sensors.cpp, control.cpp (controlPump), logger.cpp (logSensors) |
| Hoạt động 8 | Phát hiện mực nước | Cảm biến nước + còi kêu liên tục khi nước cạn; bơm luôn bị chặn khi hết nước (cả chế độ thủ công) | sensors.cpp, control.cpp (controlAlerts, controlPump) |
| Hoạt động 9 | Đèn tăng trưởng thực vật | Khi tối: LED 1&3 đỏ, LED 2&4 xanh dương; khi sáng: tắt | control.cpp (controlRgb), actuators.cpp (setRgbPlant) |
| Hoạt động 10 | Cho ăn tự động | Theo tài liệu (lesson8), nội dung là tưới nước tự động theo độ ẩm đất — dùng chung logic với Hoạt động 7; code mô phỏng thêm cửa cho ăn bằng servo (mở 2 giây rồi đóng) với nút "Cho ăn" và endpoint `/feed` | control.cpp (startFeed/controlFeed), webserver.cpp (handleFeed) |
| Hoạt động 11 | Màn hình OLED hiển thị thời tiết | OLED hiển thị nhiệt độ, độ ẩm, ánh sáng, độ ẩm đất, mực nước, **trạng thái thời tiết** (Nắng/Âm u/Mưa/Bình thường), IP WiFi và trạng thái demo | oled.cpp, sensors.cpp (weatherStatus) |
| Hoạt động 12 | Internet điều khiển vạn vật từ xa | WebServer + API + lệnh `/on` `/off` bật/tắt đèn LED từ xa | webserver.cpp (handleLedOn/handleLedOff) |
| Hoạt động 13 | Sử dụng smart phone để điều khiển nông trại | App ACEBOTT chính hãng cần chương trình riêng; dự án dùng giao diện web tương đương để điều khiển từ điện thoại | webserver.cpp |

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

### Bước 3: Hiệu chuẩn phần cứng (tùy chọn — file `src/config.h`)

Các module cảm biến/relay/còi bán kèm kit có thể hoạt động ngược chiều so với tài liệu. Thay vì sửa logic, chỉ cần đổi cờ trong `src/config.h` (đặt `1` nếu cần):

| Cờ | Mặc định | Khi nào đặt `1` |
|----|----------|-----------------|
| `INVERT_LIGHT` | `0` | Che tay mà giá trị ánh sáng **tăng** |
| `INVERT_SOIL` | `0` | Đất khô mà giá trị độ ẩm đất **tăng** |
| `INVERT_WATER` | `0` | Đổ nước vào mà giá trị mức nước **giảm** |
| `RELAY_ACTIVE_LOW` | `0` | Module relay kích mức **LOW** (bơm chạy khi chân ở LOW) |
| `BUZZER_PASSIVE` | `0` | Còi loại **passive** (không kêu bằng mức điện áp, cần xung `tone()`) |

Góc servo cũng chỉnh được để khớp cơ khí (tránh kẹt bánh răng):

| Cờ | Mặc định | Ý nghĩa |
|----|----------|---------|
| `WINDMILL_MIN_ANGLE` / `WINDMILL_MAX_ANGLE` | `0` / `180` | Biên quét của cối xay gió (HĐ6) |
| `FEED_OPEN_ANGLE` / `FEED_CLOSE_ANGLE` | `90` / `0` | Góc mở/đóng cửa cho ăn (HĐ10) |

**Cách kiểm tra nhanh chiều cảm biến:** mở Serial Monitor (115200), gõ `1` (menu debug cảm biến), che sáng / đổ nước / chạm đất và xem giá trị trên dòng `Light/Soil/Water` tăng hay giảm, rồi đặt cờ tương ứng và nạp lại.

**Bảo mật web (tùy chọn):** Đặt `WEB_TOKEN` trong `src/config.h` để bật mật khẩu cho mọi lệnh đổi trạng thái **và cả `/api`** (API trả dữ liệu cảm biến + nonce chống replay, nên không để hở). Sau khi nạp, mở `http://<IP>/?token=mat-khau-cua-ban` một lần để trang tự nhớ token (lưu `sessionStorage` và xóa token khỏi thanh địa chỉ), sau đó trang gửi token qua header `X-Auth-Token` thay vì để lộ trên URL; client tự viết vẫn có thể dùng `?token=`. Token được so sánh kiểu constant-time. Code cũng tự chặn CSRF: yêu cầu có Origin/Referer khác nguồn hoặc `Sec-Fetch-Site: cross-site` bị trả 403 (nhờ `server.collectHeaders()` trong `setup()` — đã có sẵn); trang chủ gửi `X-Frame-Options: DENY` và meta `no-referrer` để tránh lộ token qua URL. Token truyền bằng HTTP thường nên chỉ chống lẫn nhau trong LAN, không thay thế HTTPS.

### Bước 4: Chọn board và cổng
- **Tools → Board:** ESP32 Dev Module
- **Tools → Port:** Cổng USB của ESP32 (ví dụ: `/dev/ttyUSB0`)

### Bước 5: Nạp code
Nhấn nút Upload (➡) hoặc **Sketch → Upload**.

---

## 10. Sử dụng dự án

### Bước 1: Xem địa chỉ IP
1. Mở **Tools → Serial Monitor**, 115200 baud.
2. Nhấn nút RESET trên ESP32.
3. Đợi thấy: `Địa chỉ IP: 192.168.1.xxx`

### Bước 2: Truy cập giao diện web
Trên điện thoại cùng mạng WiFi, mở trình duyệt, gõ địa chỉ IP. Trang web hiển thị:
- **Cảm biến thực tế:** Nhiệt độ, độ ẩm không khí, ánh sáng, độ ẩm đất, mức nước, chuyển động, chạm, thời tiết (suy ra từ ánh sáng + độ ẩm).
- **Điều khiển thiết bị:** Máy bơm, đèn xanh (Bật/Tắt nhanh + nút Bật/Tắt riêng), màu RGB, servo (cửa), cối xay gió, cho ăn tự động.
- **Chế độ tự động:** Bật/tắt tự động cho bơm, đèn, RGB, quạt, đèn thở, kèm nút **Trình diễn hoạt động**.

**Các lệnh API điều khiển từ xa (Hoạt động 12):**

| Lệnh | Công dụng |
|------|-----------|
| `http://<IP>/on` | Bật đèn LED xanh từ xa |
| `http://<IP>/off` | Tắt đèn LED xanh từ xa |
| `http://<IP>/api` | Xem dữ liệu cảm biến dạng JSON (kèm thời tiết, trạng thái demo) |
| `http://<IP>/windmill?on=true` | Cho servo quay như cối xay gió (HĐ6); `on=false` để dừng |
| `http://<IP>/feed` | Mở cửa cho ăn tự động 2 giây rồi đóng lại (HĐ10) |
| `http://<IP>/demo` | Chạy trình diễn lần lượt các hoạt động HĐ2–HĐ12 (~40 giây) |
| `http://<IP>/toggle?id=led` | Đảo trạng thái đèn LED (pump/fan/breathe tương tự) |
| `http://<IP>/auto?id=light&on=true` | Bật/tắt chế độ tự động cho đèn (pump/rgb/fan/breathe tương tự) |

### Trình diễn hoạt động (Demo mode)

Bấm nút **Trình diễn hoạt động** trên web (hoặc gõ `http://<IP>/demo`, hoặc bấm phím `9` trên Serial Monitor) để chạy lần lượt hiệu ứng của từng hoạt động: thắp sáng → đèn thở → chạm bật/tắt → PIR → cối xay gió → tưới tự động → còi báo nước → đèn tăng trưởng → cho ăn → OLED. Trong lúc demo, hệ thống tạm dừng điều khiển tự động để tránh xung đột; màn hình OLED và Serial hiển thị tên bước đang chạy.

> **Lưu ý:** Khi bạn nhấn nút **Bật/Tắt** trên web (hoặc dùng cảm biến chạm, hoặc gõ `/on` `/off`), chế độ **tự động** của thiết bị đó sẽ tự tắt để tránh việc hệ thống tự động bật/tắt "giằng co" với lệnh thủ công của bạn. Muốn quay lại tự động, hãy nhấn nút **Tự động** tương ứng.

### Bước 3: Theo dõi OLED
Màn hình OLED hiển thị 8 dòng: tên hệ thống, trạng thái thời tiết, nhiệt độ + độ ẩm, ánh sáng, độ ẩm đất, mức nước + trạng thái bơm, địa chỉ IP WiFi, và trạng thái cối xay gió/cho ăn/demo.

---

## 11. Các tính năng an toàn đã tích hợp trong code

| Tính năng | Mô tả | Vị trí code |
|-----------|-------|-------------|
| Kiểm tra nước trước khi bơm | Không bật máy bơm nếu nước cạn (bảo vệ bơm) — áp dụng **cả khi đang bật bơm thủ công** | `control.cpp: controlPump()` |
| Hysterese độ ẩm đất | Tránh bơm nhấp nháy khi đất dao động quanh ngưỡng | `control.cpp: controlPump()` |
| Chống dội + phát hiện sườn nút chạm | Mỗi lần chạm chỉ bật/tắt đèn LED đúng 1 lần và tự chuyển sang thủ công | `control.cpp: controlTouch()` |
| Điều khiển thủ công tắt tự động | Bấm Bật/Tắt trên web (hoặc `/on` `/off`) sẽ tự tắt chế độ tự động của thiết bị đó | `webserver.cpp: handleToggle(), handleLedOn(), handleLedOff()` |
| Còi báo theo trạng thái | Nước cạn kêu còi liên tục; chỉ ghi log 1 lần mỗi sự kiện (không spam Serial) | `control.cpp: controlAlerts()` |
| Bộ đệm trung bình động 5 mẫu | Lọc nhiễu cho toàn bộ cảm biến | `sensors.cpp` |
| In dữ liệu cảm biến định kỳ | Mỗi giây in 1 dòng cảm biến ở mức DEBUG (giống bài đọc độ ẩm đất qua Serial); không gây nhiễu log ở mức INFO | `logger.cpp: logSensors()`, `SmartFarm.ino` |
| Tự kết nối lại WiFi | Thử lại ở mọi trạng thái mất mạng, không lặp log gây nhiễu | `wifi.cpp: manageWiFi()` |
| Lưu cài đặt vào flash | Không mất cài đặt khi mất điện | `settings.cpp` |
| State tập trung | Mọi biến mutable gom vào một `SystemState` (`sys.*`) duy nhất: rõ chủ sở hữu, dễ reset, không còn biến rời rạc | `globals.h`, `globals.cpp` |
| Không lưu plaintext secret | `webToken`/`otaPassword` chỉ nằm ở firmware (compile-time), không ghi vào NVS/flash | `settings.cpp`, `src/config.h` |
| Kiểm tra lỗi DHT11 | Giữ giá trị cũ khi đọc lỗi, đếm số lần lỗi để báo mã 501 | `sensors.cpp: readAllSensors()` |
| Giới hạn giá trị từ web | Chặn servo 0–180°, màu RGB 0–255, ngưỡng trong khoảng hợp lệ | `webserver.cpp` |
| Watchdog an toàn | Nạp watchdog trong lúc chờ WiFi, tự khởi động lại nếu chương trình treo | `SmartFarm.ino` |
| Servo dùng chung an toàn | Cối xay gió, cho ăn và điều khiển servo thủ công tự loại trừ nhau, không tranh chấp servo | `control.cpp: setWindmill(), startFeed()`, `webserver.cpp: handleServo()` |
| Demo không xung đột | Khi chạy trình diễn, toàn bộ điều khiển tự động tạm dừng; kết thúc demo tự tắt mọi thiết bị | `SmartFarm.ino`, `demo.cpp` |
| Mật khẩu web tùy chọn | Đặt `WEB_TOKEN` trong `src/config.h` để mọi lệnh đổi trạng thái và `/api` phải kèm token đúng (header `X-Auth-Token` hoặc `?token=`); so sánh constant-time; để trống = tắt (mặc định) | `src/config.h`, `webserver.cpp: checkToken(), guardWeb()` |
| Ghi flash tiết kiệm | `saveSettings()` chỉ ghi các khóa NVS thực sự đổi — bấm nút liên tục không mài mòn flash vô ích | `settings.cpp: saveSettings()` |
| Chống CSRF | Origin/Referer khác nguồn hoặc `Sec-Fetch-Site: cross-site` bị trả 403; yêu cầu cùng nguồn được phép | `webserver.cpp: guardWeb()`, `SmartFarm.ino: collectHeaders()` |
| Chống clickjacking/lộ token | Trang chủ gửi `X-Frame-Options: DENY` và meta `no-referrer` nên token không lộ qua URL khi mở tài nguyên ngoài | `webserver.cpp: handleRoot()` |

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

### Xem dữ liệu cảm biến trên Serial (Hoạt động 7 và 11)

Khi đổi `gMinLogLevel` thành `LOG_DEBUG`, mỗi giây Serial sẽ in thêm một dòng dữ liệu cảm biến (giống bài đọc độ ẩm đất qua cổng nối tiếp trong tài liệu):

```
[SENSORS] T:28.5C H:65.0% Light:312 Soil:640 Water:1850 PIR:0 Touch:0
```

### Bảng mã lỗi

| Mã lỗi | Ý nghĩa | Cách khắc phục |
|--------|---------|----------------|
| 101 | OLED không phát hiện được | Kiểm tra dây SDA (GPIO21), SCL (GPIO22) và địa chỉ I2C (0x3C) |
| 102 | Không thể kết nối WiFi qua 30 giây | Kiểm tra tên/mật khẩu WiFi trong `src/config.cpp`, tín hiệu WiFi; hệ thống vẫn tự thử lại nền |
| 201 | Nước cạn, tắt máy bơm để bảo vệ thiết bị | Thêm nước vào thùng, kiểm tra cảm biến mức nước (GPIO 36) |
| 202 | Đất khô, bật máy bơm tưới | Thông báo bình thường, không cần can thiệp |
| 203 | Đất đủ ẩm, tắt máy bơm | Thông báo bình thường, không cần can thiệp |
| 301 | Có chuyển động phát hiện — tự bật đèn LED xanh, người rời đi thì tắt | Thông báo bình thường từ cảm biến PIR (GPIO 33) |
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
| Còi kêu liên tục | Nước cạn trong bể | Thêm nước vào bể, kiểm tra cảm biến mực nước |
| Đèn xanh không tắt khi có người | PIR còn giữ mức HIGH (thời gian trễ của cảm biến) | Bình thường; PIR chỉ trả về LOW sau khi người rời đi hết thời gian trễ |
| Cảm biến đọc sai | Dây lỏng hoặc cảm biến hỏng | Kiểm tra dây nối, thử cảm biến khác |

---

## 14. Mở rộng nâng cao (từ tài liệu)

- Tích hợp AI dự đoán bệnh cây
- Hệ thống cảnh báo qua điện thoại (push notification)
- Điều khiển từ xa bằng app di động
- Ứng dụng năng lượng mặt trời (tấm pin mặt trời đã có trong kit)
- Thêm quạt điều khiển qua relay (relay có thể điều khiển bơm, quạt, đèn)

---

## 15. Kiểm thử tự động trên máy tính (không cần phần cứng)

Repo có sẵn bộ test chuyên sâu trong thư mục `test/`: nó biên dịch **chính firmware thật** (`SmartFarm.ino` + `src/*.cpp`) bằng `g++` trên PC, thay các thư viện ESP32 bằng mock điều khiển được (thời gian, ADC, chân GPIO, WiFi, web server, OLED...), rồi chạy **772.448 lượt kiểm tra** trên mỗi bản build thường (bản `webtoken` kiểm tra thêm bảo vệ token: 772.465 lượt). Các test gọi thẳng `runAutomation()` — hàm mà `loop()` dùng — nên không có logic nào bị sao chép khỏi firmware.

```bash
./test/run_tests.sh --quick      # 1 bản build nhanh (core 3.x, cấu hình kit)
./test/run_tests.sh --full       # 5 bản build: core 3.x, core 2.x + cảm biến đảo,
                                 # còi passive + relay active-LOW, ASan + UBSan, token web
./test/run_tests.sh --coverage   # đo độ phủ dòng lệnh + nhánh bằng gcov (hiện 99,9% dòng, ~79,7% nhánh)
```

Bộ test bao phủ: logic bơm/quạt/đèn/chạm/còi, so sánh ngưỡng đúng biên, thời gian chính xác từng ms, 15 route HTTP (kèm `/wifi` cấu hình động và callback OTA), hợp đồng JSON 21 khóa, menu serial, trình diễn 15 bước, bền vững sau khi mất điện, reset về mặc định, bỏ qua ghi flash khi giá trị không đổi, fuzz 90.000 bước với các bất biến an toàn, trường hợp `millis()` tràn số, chặn CSRF và bảo vệ token web tùy chọn (gồm cả `/api`). Chi tiết xem `test/README.md`.

Muốn hiểu **từng dòng code** làm gì (giải thích tiếng Việt, trích dẫn theo số dòng), xem [`GIAI_THICH_CODE.md`](GIAI_THICH_CODE.md) — đã phủ toàn bộ `SmartFarm.ino`, `src/`, trang web nhúng và cả bộ test (`test/`).

---

## 16. Tác giả và bản quyền

Dự án được xây dựng cho bộ starter kit **QE002 ACEBOTT IOT Smart Farm**. Code được viết và tối ưu hóa để chạy trên Arduino IDE với mạch ESP32, toàn bộ tài liệu bằng tiếng Việt có dấu. Tham khảo từ tài liệu chính thức của ACEBOTT.

Chúc bạn thành công với vườn nông trồng thông minh! 🌱