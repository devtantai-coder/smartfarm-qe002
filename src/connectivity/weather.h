#ifndef SMARTFARM_WEATHER_H
#define SMARTFARM_WEATHER_H

#include <Arduino.h>
#include "../config.h"

struct WeatherData {
  bool valid;
  float temp;
  float hum;
  int code;
  unsigned long updatedMs;
  char description[32];
};

void weatherInit();
void weatherLoop(unsigned long now);
// Bản chụp trạng thái (chép dưới khoá). Không trả tham chiếu: tác vụ mạng nền
// ghi sWeather song song với loopTask đọc.
WeatherData weatherState();

// Tải dữ liệu thời tiết ngay (blocking). Chỉ tác vụ mạng nền gọi hàm này;
// weatherLoop() chỉ xếp job rồi trả về để loop() không bị chặn.
bool weatherFetchNow();

const char* weatherCodeDescription(int code);
int buildWeatherJson(char* buf, size_t cap);

#ifdef SMARTFARM_TEST_HOOKS
void testResetWeatherStatics();
bool testParseWeatherBody(const char* body, unsigned long now);
#endif

#endif
