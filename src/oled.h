#ifndef OLED_H
#define OLED_H

void updateOLED(float t, float h, int light, int soil, int water);

// Bắt buộc lần updateOLED() kế tiếp đẩy lại TOÀN BỘ khung hình: gọi sau khi
// code khác vẽ trực tiếp lên màn hình (màn hình test của debugOled) để bộ đệm
// so sánh delta không tưởng nhầm "không đổi" rồi bỏ qua.
void oledForceRedraw();

#endif
