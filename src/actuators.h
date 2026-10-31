#ifndef ACTUATORS_H
#define ACTUATORS_H

void setRelay(bool on);
void setFan(bool on);
void setFanPwm(uint8_t duty);   // duty là công suất LOGIC: 0 = tắt, 255 = hết tốc
void setBuzzer(bool on);        // ghi trực tiếp, dùng cho trình tự test cơ cấu
// Còi có hai chủ riêng biệt: "alarm" là cảnh báo thật (nước cạn, còi người dùng
// bật) còn "beep" chỉ là hiệu ứng âm thanh của trình diễn. Alarm luôn thắng, nên
// một bước demo không thể tắt cảnh báo đang sống.
void setBuzzerAlarm(bool on);
void setBuzzerBeep(bool on);
void setBlueLed(bool on);
void setBreatheLed(bool on);
void setLedPwm(uint8_t value);
void setRgb(uint8_t r, uint8_t g, uint8_t b);
void setRgbPlant();
// true khi dải LED đang hiển thị hoa văn đèn tăng trưởng (đỏ/xanh xen kẽ),
// false khi đang hiển thị một màu đơn sắc. Dùng để biết chắc trạng thái thật
// của dải LED thay vì suy đoán từ giá trị RGB (màu đơn (255,0,255) trùng
// với giá trị RGB của hoa văn).
bool rgbPlantPatternActive();
void setRgbAuto(bool on);
void setPumpAuto(bool on);
void setFanAuto(bool on);
void setLightAuto(bool on);
void setBreatheAuto(bool on);

#endif
