#ifndef SMARTFARM_OTA_H
#define SMARTFARM_OTA_H
#include <Arduino.h>
void otaInit();
void otaLoop();
bool otaVerifySignature(const unsigned char* data, size_t len, const unsigned char* sig, size_t sigLen);
#endif
