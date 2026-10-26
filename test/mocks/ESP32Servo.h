#pragma once
#include "Arduino.h"

void mockServoWrite(int angle);

class Servo {
public:
  void attach(int) {}
  void write(int angle) { mockServoWrite(angle); }
};
