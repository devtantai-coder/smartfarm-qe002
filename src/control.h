#ifndef CONTROL_H
#define CONTROL_H

void controlPump();
void controlFan();
void controlLight();
void controlRgb();
void controlBreathe();
void controlTouch();
void controlAlerts();

// Servo mechanisms: windmill (activity 6) + feed gate (activity 10)
void setWindmill(bool on);
void controlWindmill();
void startFeed();
void stopFeed();
void controlFeed();
bool feedRunning();

#endif
