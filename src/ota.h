#ifndef SMARTFARM_OTA_H
#define SMARTFARM_OTA_H
#include <Arduino.h>
// OTA của firmware này dùng ArduinoOTA (xem SmartFarm.ino): chỉ bật khi có
// OTA_PASSWORD. Không có bước kiểm tra chữ ký ảnh firmware — hàm
// otaVerifySignature() cũ luôn trả false và không nơi nào gọi, nên đã bỏ để
// không tạo cảm giác an toàn giả.
void otaInit();
void otaLoop();
#endif
