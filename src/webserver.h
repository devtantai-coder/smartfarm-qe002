// Không dùng guard WEBSERVER_H: thư viện WebServer của core ESP32 cũng dùng
// đúng macro này, include <WebServer.h> trước sẽ khiến toàn bộ header này
// bị bỏ qua và mọi handler "not declared".
#ifndef SMARTFARM_WEBSERVER_H
#define SMARTFARM_WEBSERVER_H

void handleRoot();
void handleApi();
void handleToggle();
void handleAuto();
void handleRgb();
void handleBreatheLed();
void handleLedOn();
void handleLedOff();
void handleServo();
void handleWindmill();
void handleFeed();
void handleDemo();
void handleThreshold();

// Guard dùng chung cho các handler ở module khác (health/metrics).
bool webTokenGuard();
bool webGuard();

#endif
