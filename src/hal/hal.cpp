#include "../hal.h"
#include "../globals.h"

void halPinModeOutput(uint8_t pin) { pinMode(pin, OUTPUT); }
void halDigitalWrite(uint8_t pin, bool high) { digitalWrite(pin, high ? HIGH : LOW); }
int  halDigitalRead(uint8_t pin) { return digitalRead(pin); }
int  halAnalogRead(uint8_t pin) { return analogRead(pin); }
void halServoWrite(int angle) { servo.write(angle); }

void halPwmWrite(uint8_t pin, uint32_t duty) { ledcWrite(pin, duty); }
int  halAnalogReadRaw(uint8_t pin) { return analogRead(pin); }
void halI2cBegin(uint8_t sda, uint8_t scl) { Wire.begin(sda, scl); }
void halSpiBegin() {}
