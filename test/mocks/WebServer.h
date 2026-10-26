#pragma once
#include "Arduino.h"

// mock WebServer: các route được ghi lại và test có thể gọi,
// tham số request được inject, response được ghi lại (xem mock.cpp/mock.h).
class WebServer {
public:
  WebServer(int) {}
  void on(const char *path, void (*handler)());
  void collectHeaders(const char *keys[], size_t count);
  void begin() {}
  void handleClient() {}
  void sendHeader(const char *name, const char *value);
  void setContentLength(size_t length);
  void send(int code, const char *type, const char *body);
  void send(int code, const char *type, const String &body);
  void send(int code);
  void sendContent(const char *content, size_t length);
  void send_P(int code, const char *type, const char *body);
  String arg(const char *name);
  bool hasArg(const char *name);
  bool hasHeader(const char *name);
  String header(const char *name);
};
