#include "../tls.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"

static bool sEnabled = false;

void tlsInit() {
#if TLS_ENABLED
  // TLS cho MQTT/WebSocket chưa được triển khai; không quảng cáo nhầm là an toàn.
  sEnabled = false;
  LOG_WARN("TLS_ENABLED=1 but TLS transport is not implemented");
#else
  sEnabled = false;
#endif
}

bool tlsEnabled() { return sEnabled; }
