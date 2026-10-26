#include "pid.h"
#include "globals.h"

void pidInit(float kp, float ki, float kd) {
  sys.pidKp = kp; sys.pidKi = ki; sys.pidKd = kd;
  sys.pidIntegral = 0; sys.pidPrevError = 0; sys.pidLastTime = millis();
}
float pidUpdate(float setpoint, float measured) {
  float err = setpoint - measured;
  unsigned long now = millis();
  float dt = (now - sys.pidLastTime) / 1000.0f;
  if (dt <= 0) dt = 0.001f;
  sys.pidIntegral += err * dt;
  float deriv = (err - sys.pidPrevError) / dt;
  float out = sys.pidKp * err + sys.pidKi * sys.pidIntegral + sys.pidKd * deriv;
  sys.pidPrevError = err;
  sys.pidLastTime = now;
  return out;
}
void pidReset() { sys.pidIntegral = 0; sys.pidPrevError = 0; }
