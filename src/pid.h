#ifndef SMARTFARM_PID_H
#define SMARTFARM_PID_H
#include <Arduino.h>
void pidInit(float kp, float ki, float kd);
float pidUpdate(float setpoint, float measured);
void pidReset();
#endif
