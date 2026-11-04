#include "tls.h"
#include "../core/globals.h"
#include "../core/logger.h"
#include "../core/config.h"

static bool sEnabled = false;

#ifndef SMARTFARM_TEST_HOOKS
#include <WiFiClientSecure.h>
#endif

void tlsInit() {
#if TLS_ENABLED
  sEnabled = true;
  LOG_INFO("TLS enabled");
#else
  sEnabled = false;
#endif
}

void tlsSetEnabled(bool enabled) {
  sEnabled = enabled;
  LOG_INFO(enabled ? "TLS enabled" : "TLS disabled");
}

bool tlsEnabled() { return sEnabled; }
