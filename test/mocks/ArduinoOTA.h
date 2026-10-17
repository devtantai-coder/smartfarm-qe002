#pragma once
#include <cstdint>

class ArduinoOTAClass {
public:
  void setHostname(const char*) {}
  void setPassword(const char*) {}
  void onStart(void (*fn)()) {}
  void onEnd(void (*fn)()) {}
  void onProgress(void (*fn)(unsigned int, unsigned int)) {}
  void onError(void (*fn)(int)) {}
  void begin() {}
  void handle() {}
};

inline ArduinoOTAClass ArduinoOTA;

typedef int ota_error_t;
static const ota_error_t OTA_AUTH_ERROR = 0;
static const ota_error_t OTA_BEGIN_ERROR = 1;
static const ota_error_t OTA_CONNECT_ERROR = 2;
static const ota_error_t OTA_RECEIVE_ERROR = 3;
static const ota_error_t OTA_END_ERROR = 4;
