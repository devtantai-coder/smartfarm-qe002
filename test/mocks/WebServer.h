#pragma once
#include "Arduino.h"

// Giá trị method mô phỏng đúng core thật (WebServer/src/HTTP_Method.h: enum
// http_method + #define HTTP_ANY (HTTPMethod)(255)) để test bắt được lỗi đăng
// ký route thiếu method — overload 2 tham số của core là HTTP_ANY, nghĩa là
// nhận cả POST và body được đọc ngay trên loopTask.
typedef enum {
  HTTP_DELETE = 0, HTTP_GET = 1, HTTP_HEAD = 2, HTTP_POST = 3,
  HTTP_PUT = 6, HTTP_PATCH = 7, HTTP_OPTIONS = 15
} HTTPMethod;
#define HTTP_ANY (HTTPMethod)(255)

// mock WebServer: các route được ghi lại (kèm method) và test có thể gọi,
// tham số request được inject, response được ghi lại (xem mock.cpp/mock.h).
class WebServer {
public:
  WebServer(int) {}
  // Hai overload đều được mô phỏng, với cùng ngữ nghĩa như core: bản 2 tham số
  // đăng ký HTTP_ANY.
  void on(const char *path, void (*handler)());
  void on(const char *path, HTTPMethod method, void (*handler)());
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
  String hostHeader();
};
