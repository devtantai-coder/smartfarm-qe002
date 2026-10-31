#ifndef SMARTFARM_HAL_H
#define SMARTFARM_HAL_H

#include <Arduino.h>

void halDigitalWrite(uint8_t pin, bool high);
int  halDigitalRead(uint8_t pin);
int  halAnalogRead(uint8_t pin);
void halServoWrite(int angle);

// (Đã bỏ halPwmWrite/halAnalogReadRaw: hàm PWM nhận "chân" chỉ đúng ở core 3.x,
// còn ở core 2.x ledcWrite() nhận KÊNH nên sẽ điều khiển nhầm kênh — và không
// nơi nào dùng. PWM thật nằm trong actuators.cpp với #if theo phiên bản core.)
// (Đã bỏ halPinModeOutput/halI2cBegin/halSpiBegin: không caller nào trong tree —
// setup() tự ghi mức rồi pinMode từng chân output, còn I2C do ui/oled.cpp gọi
// Wire.begin() trực tiếp kèm tham số timeout/speed riêng.)

#endif
