#ifndef CONTROL_H
#define CONTROL_H

void controlPump();
void controlFan();
void controlLight();
void controlRgb();
void controlBreathe();
void controlTouch();
void controlAlerts();
void setManualAlarm(bool on);
bool manualAlarm();
// Cơ cấu servo: cối xay gió (bài 6) + cửa cho ăn (bài 10)
void setWindmill(bool on);
void controlWindmill();
void startFeed();
void stopFeed();
void controlFeed();
bool feedRunning();

#endif
