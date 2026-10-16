#pragma once
#include "Arduino.h"

// WebServer mock: routes are recorded and can be invoked by the tests,
// request args are injected, responses are captured (see mock.cpp/mock.h).
class WebServer {
public:
  WebServer(int) {}
  void on(const char *path, void (*handler)());
  void begin() {}
  void handleClient() {}
  void send(int code, const char *type, const char *body);
  void send(int code, const char *type, const String &body);
  void send(int code);
  void send_P(int code, const char *type, const char *body);
  String arg(const char *name);
  bool hasArg(const char *name);
};
