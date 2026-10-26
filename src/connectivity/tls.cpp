#include "../tls.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"

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

bool tlsEnabled() { return sEnabled; }
