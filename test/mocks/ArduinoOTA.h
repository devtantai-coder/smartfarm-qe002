#pragma once
#include <cstdint>

// Mock OTA: ghi nhớ callback do setup() đăng ký để test gọi lại
// qua mockOta*() và kiểm tra đúng nội dung in ra serial.
// Số lần ArduinoOTA.handle() được gọi (đo tần số xử lý gói OTA).
extern int mockOtaHandleCalls;

class ArduinoOTAClass {
public:
  void (*startFn)() = nullptr;
  void (*endFn)() = nullptr;
  void (*progressFn)(unsigned int, unsigned int) = nullptr;
  void (*errorFn)(int) = nullptr;
  void setHostname(const char*) {}
  void setPassword(const char*) {}
  void onStart(void (*fn)()) { startFn = fn; }
  void onEnd(void (*fn)()) { endFn = fn; }
  void onProgress(void (*fn)(unsigned int, unsigned int)) { progressFn = fn; }
  void onError(void (*fn)(int)) { errorFn = fn; }
  void begin() {}
  void handle() { mockOtaHandleCalls++; }
};

inline ArduinoOTAClass ArduinoOTA;

inline void mockOtaStart() { if (ArduinoOTA.startFn) ArduinoOTA.startFn(); }
inline void mockOtaEnd() { if (ArduinoOTA.endFn) ArduinoOTA.endFn(); }
inline void mockOtaProgress(unsigned int p, unsigned int total) {
  if (ArduinoOTA.progressFn) ArduinoOTA.progressFn(p, total);
}
inline void mockOtaError(int e) { if (ArduinoOTA.errorFn) ArduinoOTA.errorFn(e); }

typedef int ota_error_t;
static const ota_error_t OTA_AUTH_ERROR = 0;
static const ota_error_t OTA_BEGIN_ERROR = 1;
static const ota_error_t OTA_CONNECT_ERROR = 2;
static const ota_error_t OTA_RECEIVE_ERROR = 3;
static const ota_error_t OTA_END_ERROR = 4;
