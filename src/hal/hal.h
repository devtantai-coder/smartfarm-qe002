#ifndef SMARTFARM_HAL_H
#define SMARTFARM_HAL_H

#include <Arduino.h>

void halPinModeOutput(uint8_t pin);
void halDigitalWrite(uint8_t pin, bool high);
int  halDigitalRead(uint8_t pin);
int  halAnalogRead(uint8_t pin);
void halServoWrite(int angle);

// Mở rộng HAL
void halPwmWrite(uint8_t pin, uint32_t duty);
int  halAnalogReadRaw(uint8_t pin);
void halI2cBegin(uint8_t sda, uint8_t scl);
void halSpiBegin();

#endif
