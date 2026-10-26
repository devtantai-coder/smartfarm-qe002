#include "../ota.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"

void otaInit() { LOG_INFO("OTA init"); }

void otaLoop() {}

bool otaVerifySignature(const unsigned char* data, size_t len, const unsigned char* sig, size_t sigLen) {
  // Chưa triển khai kiểm tra chữ ký. Trả false để không bao giờ chấp nhận
  // firmware chưa được xác thực; bật OTA qua ArduinoOTA (có mật khẩu) thay thế.
  (void)data; (void)len; (void)sig; (void)sigLen;
  return false;
}
