#pragma once
#include "Arduino.h"

#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_SCAN_COMPLETED 2
#define WL_CONNECTED 3
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5
#define WL_DISCONNECTED 6

extern int mockWifiStatus;
extern const char *mockWifiIp;
extern int mockWifiBeginCalls, mockWifiDisconnectCalls, mockWifiReconnectCalls;
extern int mockWifiPersistentCalls, mockWifiSleepDisableCalls;
extern int mockWifiStatusCalls;      // số lần gọi WiFi.status() (đo tần số hỏi driver)

class IPAddress {
public:
  // Chốt giá trị lúc tạo để so sánh ==/!= hoạt động giống IPAddress thật
  // (code sản phẩm dùng phép so sánh để cache tiền tố origin).
  IPAddress() : s_(mockWifiIp ? mockWifiIp : "") {}
  String toString() const { return String(s_.c_str()); }
  bool operator==(const IPAddress &o) const { return s_ == o.s_; }
  bool operator!=(const IPAddress &o) const { return s_ != o.s_; }
private:
  std::string s_;
};

struct WiFiStub {
  int status() { mockWifiStatusCalls++; return mockWifiStatus; }
  IPAddress localIP() { return IPAddress(); }
  String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
  int RSSI() { return -55; }
  void begin(const char *, const char *);
  void disconnect();
  void reconnect();
  void setSleep(bool on) { if (!on) mockWifiSleepDisableCalls++; }
  void persistent(bool on) { if (!on) mockWifiPersistentCalls++; }
};
extern WiFiStub WiFi;
