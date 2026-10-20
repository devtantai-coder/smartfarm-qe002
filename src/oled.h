#ifndef OLED_H
#define OLED_H

#ifndef SMARTFARM_OLED_H
#define SMARTFARM_OLED_H

// Khởi tạo I2C + màn hình OLED.
// Trả false nếu không tìm thấy màn hình — khi đó firmware KHÔNG đụng I2C nữa.
// Lý do: mỗi giao dịch I2C không có thiết bị phản hồi sẽ chặn tới hết
// thời gian chờ (mặc định 50 ms), biến mỗi lần vẽ OLED thành một nhịp chặn
// dài của loop().
bool oledInit();

// true nếu màn hình còn dùng được (không bị tắt do lỗi I2C).
bool oledAvailable();

// Bắt buộc lần updateOLED() kế tiếp đẩy lại TOÀN BỘ khung hình: gọi sau khi
// code khác vẽ trực tiếp lên màn hình (màn hình test của debugOled) để bộ đệm
// so sánh delta không tưởng nhầm "không đổi" rồi bỏ qua.
void oledForceRedraw();

void updateOLED(float t, float h, int light, int soil, int water);

#endif

#endif
