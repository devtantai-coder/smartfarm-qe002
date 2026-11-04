#ifndef ACTUATORS_H
#define ACTUATORS_H

void setRelay(bool on);
void setFan(bool on);
void setBuzzer(bool on);
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
