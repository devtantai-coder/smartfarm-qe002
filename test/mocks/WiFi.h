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

class IPAddress {
public:
  String toString() const { return String(mockWifiIp); }
};

struct WiFiStub {
  int status() { return mockWifiStatus; }
  IPAddress localIP() { return IPAddress(); }
  String macAddress() { return String("AA:BB:CC:DD:EE:FF"); }
  int RSSI() { return -55; }
  void begin(const char *, const char *);
  void disconnect();
  void reconnect();
  void setSleep(bool) {}
};
extern WiFiStub WiFi;
