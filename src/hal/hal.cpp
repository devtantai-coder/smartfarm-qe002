#include "../hal.h"
#include "../globals.h"

void halDigitalWrite(uint8_t pin, bool high) { digitalWrite(pin, high ? HIGH : LOW); }
int  halDigitalRead(uint8_t pin) { return digitalRead(pin); }
int  halAnalogRead(uint8_t pin) { return analogRead(pin); }
void halServoWrite(int angle) { servo.write(angle); }
