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
void handleWifi();
void handleReset();
void handleAppConfig();
void handleAppAll();
void handleAppDoor();
void handleAppLed();
void handleHistory();
void handleWeather();

// Guard dùng chung cho các handler ở module khác (health/metrics).
bool webTokenGuard();
bool webGuard();

// Đọc tham số float bắt buộc là số hữu hạn (khác toFloat() trả 0 cho "abc").
bool webFloatArg(const char* name, float* out);

// true khi app ACEBOTT vừa điều khiển; automation tạm dừng vài giây.
bool appControlActive();

#endif
