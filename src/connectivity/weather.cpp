#include "weather.h"
#include "../logger.h"
#include "../wifi.h"
#include "../globals.h"
#include "../irqlock.h"
#include "networker.h"
#include <WiFi.h>

#if !defined(SMARTFARM_TEST_HOOKS)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

#if WEATHER_ENABLED

// sWeather do tác vụ mạng nền ghi, loopTask đọc (route /weather, lịch cập nhật
// trong weatherLoop). Mọi truy cập cross-task phải nằm trong IrqGuard.
static WeatherData sWeather = {};
static IrqLock sWeatherLock = IRQLOCK_INIT;
static bool sHasAttempt = false;
static unsigned long sLastAttemptMs = 0;

const char* weatherCodeDescription(int code) {
  if (code == 0) return "Trời quang";
  if (code == 1) return "Ít mây";
  if (code == 2) return "Có mây";
  if (code == 3) return "Nhiều mây";
  if (code == 45 || code == 48) return "Sương mù";
  if (code >= 51 && code <= 57) return "Mưa phùn";
  if (code >= 61 && code <= 67) return "Mưa";
  if (code >= 71 && code <= 77) return "Tuyết";
  if (code >= 80 && code <= 82) return "Mưa rào";
  if (code >= 85 && code <= 86) return "Mưa tuyết";
  if (code >= 95 && code <= 99) return "Giông bão";
  return "Không rõ";
}

static bool extractNumber(const char* body, const char* key, double* out) {
  if (!body || !key || !out) return false;
  const char* p = strstr(body, key);
  if (!p) return false;
  p += strlen(key);
  while (*p == ' ' || *p == '\t') p++;
  char* end = nullptr;
  const double v = strtod(p, &end);
  if (end == p) return false;
  // strtod chấp nhận cả "nan"/"inf"; số không hữu hạn sẽ in ra "nan" trong
  // JSON /weather (không phải JSON hợp lệ) và đầu độc mọi phép so sánh.
  if (!isfinite(v)) return false;
  *out = v;
  return true;
}

static bool parseWeatherBody(const char* body, unsigned long now) {
  if (!body) return false;
  // Bỏ qua current_units (cùng tên khóa nhưng giá trị là chuỗi) bằng cách
  // chỉ tìm trong object "current".
  const char* current = strstr(body, "\"current\":");
  if (!current) current = body;

  double temp = 0, hum = 0, code = 0;
  if (!extractNumber(current, "\"temperature_2m\":", &temp)) return false;
  const bool hasHum = extractNumber(current, "\"relative_humidity_2m\":", &hum);
  const bool hasCode = extractNumber(current, "\"weather_code\":", &code);

  // Dựng xong toàn bộ bản chụp mới rồi MỘT lần chép ra ngoài dưới khoá. Cách cũ
  // gán trực tiếp sWeather.valid = true rồi mới ghi từng field: loopTask trông
  // thấy valid=true kèm temp/hum/description còn đang viết dở.
  WeatherData fresh = {};
  fresh.valid = true;
  fresh.temp = (float)temp;
  fresh.hum = hasHum ? (float)hum : 0.0f;
  fresh.code = hasCode ? (int)code : -1;
  fresh.updatedMs = now;
  strncpy(fresh.description, weatherCodeDescription(fresh.code), sizeof(fresh.description) - 1);
  fresh.description[sizeof(fresh.description) - 1] = '\0';

  {
    IrqGuard g(&sWeatherLock);
    sWeather = fresh;
  }
  return true;
}

#if !defined(SMARTFARM_TEST_HOOKS)
// Chạy trên tác vụ mạng nền (xem networker.cpp). Đọc theo CHUNK và nhường CPU
// giữa các lần chờ: trước đây vòng lặp đọc từng byte bằng client.read() trong
// lúc chờ mạng, chiếm trọn CPU của loopTask suốt thời gian timeout.
bool weatherFetchNow() {
  if (!wifiConnectedFresh()) return false;

  WiFiClient client;
  if (!client.connect("api.open-meteo.com", 80, (int)WEATHER_TIMEOUT_MS)) {
    LOG_WARN("Weather: cannot connect");
    return false;
  }

  char request[320];
  const int reqLen = snprintf(request, sizeof(request),
                              "GET /v1/forecast?latitude=%s&longitude=%s"
                              "&current=temperature_2m,relative_humidity_2m,weather_code HTTP/1.1\r\n"
                              "Host: api.open-meteo.com\r\n"
                              "Connection: close\r\n\r\n",
                              WEATHER_LATITUDE, WEATHER_LONGITUDE);
  if (reqLen <= 0 || (size_t)reqLen >= sizeof(request)) {
    client.stop();
    return false;
  }
  client.print(request);

  char response[2048];
  size_t len = 0;
  const unsigned long deadline = millis() + WEATHER_TIMEOUT_MS;
  while (len < sizeof(response) - 1 && (long)(deadline - millis()) > 0) {
    const int avail = client.available();
    if (avail <= 0) {
      if (!client.connected()) break;
      vTaskDelay(1);                  // nhường CPU; không busy-wait
      continue;
    }
    size_t chunk = (size_t)avail;
    if (chunk > sizeof(response) - 1 - len) chunk = sizeof(response) - 1 - len;
    const int got = client.read((uint8_t*)response + len, chunk);
    if (got > 0) len += (size_t)got;
  }
  response[len] = '\0';
  client.stop();

  char* body = strstr(response, "\r\n\r\n");
  body = body ? body + 4 : response;
  if (!parseWeatherBody(body, millis())) {
    LOG_WARN("Weather: invalid response");
    return false;
  }
  LOG_INFO("Weather: updated");
  return true;
}
#else
bool weatherFetchNow() { return false; }
#endif  // !SMARTFARM_TEST_HOOKS

void weatherInit() {
  {
    IrqGuard g(&sWeatherLock);
    sWeather = {};
    strncpy(sWeather.description, "Chưa có dữ liệu", sizeof(sWeather.description) - 1);
    sWeather.description[sizeof(sWeather.description) - 1] = '\0';
  }
  sHasAttempt = false;
  sLastAttemptMs = 0;
}

// Chỉ kiểm tra "đến giờ chưa" rồi xếp job; việc tải thật do tác vụ mạng nền
// làm để loop() không bao giờ phải chờ HTTP.
void weatherLoop(unsigned long now) {
#if !defined(SMARTFARM_TEST_HOOKS)
  if (!wifiConnected()) return;
  const unsigned long retryMs = weatherState().valid ? WEATHER_REFRESH_MS : 60000UL;
  if (sHasAttempt && now - sLastAttemptMs < retryMs) return;
  sHasAttempt = true;
  sLastAttemptMs = now;
  // Post fail (queue đầy, chưa tạo được queue) vẫn GIỮ throttle: lần thử kế
  // tiếp theo retryMs. Reset cờ ở đây từng làm weatherLoop quay theo nhịp
  // loop, lấy spinlock + gọi post hàng nghìn lần/giây khi queue hỏng.
  if (!netWorkerPostWeather()) LOG_DEBUG("Weather: job not queued, retry later");
#else
  (void)now;
#endif
}

// Trả BẢN CHÚP theo giá trị, không trả tham chiếu: tham chiếu vào sWeather cho
// phép caller đọc tự do ngoài khoá, đúng là race mà ta vừa khóa lại.
WeatherData weatherState() {
  IrqGuard g(&sWeatherLock);
  return sWeather;
}

// Float vô hạn/NaN (server trả "1e999", ép float tràn) in ra "inf"/"nan" làm
// JSON /weather không parse được; kẹp về 0 trước khi in như các route khác.
static double printableWeather(float v) {
  return isfinite(v) ? (double)v : 0.0;
}

int buildWeatherJson(char* buf, size_t cap) {
  const WeatherData w = weatherState();
  const unsigned long age = w.valid ? (millis() - w.updatedMs) / 1000UL : 0;
  return snprintf(buf, cap,
                  "{\"valid\":%s,\"temp\":%.1f,\"hum\":%.0f,\"code\":%d,"
                  "\"desc\":\"%s\",\"age\":%lu}",
                  w.valid ? "true" : "false", printableWeather(w.temp),
                  printableWeather(w.hum), w.code, w.description,
                  age);
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetWeatherStatics() { weatherInit(); }
bool testParseWeatherBody(const char* body, unsigned long now) {
  return parseWeatherBody(body, now);
}
#endif

#else  // WEATHER_ENABLED

void weatherInit() {}
void weatherLoop(unsigned long) {}
WeatherData weatherState() { return {}; }
bool weatherFetchNow() { return false; }
const char* weatherCodeDescription(int) { return "Tắt"; }
int buildWeatherJson(char* buf, size_t cap) {
  return snprintf(buf, cap, "{\"valid\":false,\"temp\":0,\"hum\":0,\"code\":-1,"
                            "\"desc\":\"Tắt\",\"age\":0}");
}
#ifdef SMARTFARM_TEST_HOOKS
void testResetWeatherStatics() {}
bool testParseWeatherBody(const char*, unsigned long) { return false; }
#endif

#endif
