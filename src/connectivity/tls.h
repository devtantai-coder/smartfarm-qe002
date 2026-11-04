#ifndef SMARTFARM_TLS_H
#define SMARTFARM_TLS_H
#include <Arduino.h>

void tlsInit();
bool tlsEnabled();

// Bật/tắt TLS ở runtime (mặc định theo TLS_ENABLED).
void tlsSetEnabled(bool enabled);

#endif
