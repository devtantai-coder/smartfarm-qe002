#ifndef SMARTFARM_HISTORY_H
#define SMARTFARM_HISTORY_H

#include <Arduino.h>
#include "../config.h"

// Một mẫu lịch sử cảm biến. Giữ kích thước nhỏ để ring buffer 96 mẫu chỉ
// chiếm khoảng 2 KB RAM.
struct HistorySample {
  uint32_t seconds;   // thời điểm lấy mẫu (giây kể từ lúc khởi động)
  float temp;
  float hum;
  uint16_t light;
  uint16_t soil;
  uint16_t water;
  uint8_t flags;      // bit0 bơm, bit1 quạt, bit2 còi báo động
};

void historyInit();
void historyLoop(unsigned long now);

int historyCount();
bool historyAt(int index, HistorySample* out);   // index 0 = mẫu cũ nhất

// Dựng JSON cho /history. Trả số byte cần (giống snprintf): nếu >= cap thì
// JSON bị cắt cụt và handler phải trả lỗi thay vì gửi JSON hỏng.
int buildHistoryJson(char* buf, size_t cap);

// Số byte thân response sẽ cần. Handler cấp phát đúng bấy nhiêu rồi dựng JSON
// vào đó trong MỘT lần ghi — xem handleHistory().
size_t historyJsonSize();

#ifdef SMARTFARM_TEST_HOOKS
void testResetHistoryStatics();
#endif

#endif
