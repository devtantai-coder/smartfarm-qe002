#include "../ota.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"

void otaInit() { LOG_INFO("OTA init"); }

void otaLoop() {}

bool otaVerifySignature(const unsigned char* data, size_t len, const unsigned char* sig, size_t sigLen) {
  if (!data || !sig || len == 0 || sigLen == 0) return false;
#ifndef SMARTFARM_TEST_HOOKS
  // Real verification would use mbedtls_pk_verify with the firmware public key.
  // Stubbed to true for development; replace with actual RSA/ECDSA check.
  (void)data; (void)len; (void)sig; (void)sigLen;
  return true;
#else
  return true;
#endif
}
