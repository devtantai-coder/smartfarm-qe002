#include "ota.h"
#include "../core/globals.h"
#include "../core/logger.h"
#include "../core/config.h"

void otaInit() { LOG_INFO("OTA init"); }

void otaLoop() {}

// Fail-closed: chưa có public key / cơ chế verify thật nên từ chối mọi
// OTA có ký. Trả true ở đây là backdoor (ai cũng nạp firmware giả được).
// Khi có public key thật, thay bằng mbedtls_pk_verify với RSA/ECDSA check
// tương ứng; cho tới lúc đó return false an toàn hơn là true.
bool otaVerifySignature(const unsigned char* data, size_t len, const unsigned char* sig, size_t sigLen) {
  (void)data; (void)len; (void)sig; (void)sigLen;
  return false;
}
