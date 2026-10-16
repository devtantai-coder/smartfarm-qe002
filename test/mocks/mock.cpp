#include "mock.h"
#include "Arduino.h"
#include "DHT.h"
#include "ESP32Servo.h"
#include "Adafruit_NeoPixel.h"
#include "Adafruit_SSD1306.h"
#include "WebServer.h"
#include "Wire.h"
#include "WiFi.h"
#include "esp_task_wdt.h"

#include <cstdarg>
#include <string>
#include <vector>

static unsigned long sMillis = 0;

float mockDhtTemp = 25.0f;
float mockDhtHum = 60.0f;
int mockAdc[40] = {0};
uint8_t mockDigital[40] = {0};
uint32_t mockPinLevel[40] = {0};
int mockServoAngle = 0;
uint32_t mockPwm[40] = {0};
int mockBuzzerToneHz = 0;
uint32_t mockPixel[8] = {0};
int mockWifiStatus = WL_CONNECTED;
const char *mockWifiIp = "192.168.1.50";
bool mockDisplayBeginOk = true;

int mockDigitalWriteCount = 0;
int mockLedcWriteCount = 0;
int mockInvalidPixelWrites = 0;
int mockInvalidPwmWrites = 0;
int mockServoWriteCount = 0;
int mockServoMinSeen = 9999;
int mockServoMaxSeen = -1;
int mockToneCalls = 0, mockNoToneCalls = 0;
int mockWifiBeginCalls = 0, mockWifiDisconnectCalls = 0, mockWifiReconnectCalls = 0;
int mockWdtInitCalls = 0, mockWdtAddCalls = 0, mockWdtResetCalls = 0;

TwoWire Wire;
WiFiStub WiFi;
SerialStub Serial;

// ── Lõi Arduino ──
unsigned long millis() { return sMillis; }
void delay(unsigned long ms) { sMillis += ms; }
unsigned long mockMillis() { return sMillis; }
void mockAdvance(unsigned long ms) { sMillis += ms; }
void mockSetMillis(unsigned long ms) { sMillis = ms; }

// ── Log thứ tự gọi IO ──
static std::string sIoLog;

static void ioLogAppend(char kind, uint8_t pin, int value) {
  char b[24];
  if (kind == 'W') snprintf(b, sizeof(b), "W:%u=%d;", (unsigned)pin, value);
  else             snprintf(b, sizeof(b), "M:%u;", (unsigned)pin);
  sIoLog += b;
}

const char *mockIoLog() { return sIoLog.c_str(); }
void mockIoLogClear() { sIoLog.clear(); }

void pinMode(uint8_t pin, uint8_t) { ioLogAppend('M', pin, 0); }
void digitalWrite(uint8_t pin, uint8_t value) {
  if (pin < 40) mockPinLevel[pin] = value;
  mockDigitalWriteCount++;
  ioLogAppend('W', pin, value);
}
int digitalRead(uint8_t pin) { return pin < 40 ? mockDigital[pin] : 0; }
int analogRead(uint8_t pin) { return pin < 40 ? mockAdc[pin] : 0; }
void ledcSetup(uint8_t, double, uint8_t) {}
void ledcAttachPin(uint8_t, uint8_t) {}
void ledcWrite(uint8_t ch, uint32_t value) {
  mockLedcWriteCount++;
  if (ch >= 40) { mockInvalidPwmWrites++; return; }
  mockPwm[ch] = value;
}
void ledcAttach(uint8_t, uint32_t, uint8_t) {}
void tone(uint8_t, unsigned int frequency, unsigned long) {
  mockToneCalls++;
  mockBuzzerToneHz = (int)frequency;
}
void noTone(uint8_t) {
  mockNoToneCalls++;
  mockBuzzerToneHz = 0;
}

// ── WiFi ──
void WiFiStub::begin(const char *, const char *) { mockWifiBeginCalls++; }
void WiFiStub::disconnect() { mockWifiDisconnectCalls++; }
void WiFiStub::reconnect() { mockWifiReconnectCalls++; }

// ── esp_task_wdt ──
int esp_task_wdt_init(const esp_task_wdt_config_t *) { mockWdtInitCalls++; return 0; }
int esp_task_wdt_init(uint32_t, bool) { mockWdtInitCalls++; return 0; }
int esp_task_wdt_add(void *) { mockWdtAddCalls++; return 0; }
int esp_task_wdt_reset() { mockWdtResetCalls++; return 0; }
int esp_task_wdt_deinit() { return 0; }

// ── DHT ──
float DHT::readTemperature() { return mockDhtTemp; }
float DHT::readHumidity() { return mockDhtHum; }

// ── Hook Servo / NeoPixel ──
void mockServoWrite(int angle) {
  mockServoAngle = angle;
  mockServoWriteCount++;
  if (angle < mockServoMinSeen) mockServoMinSeen = angle;
  if (angle > mockServoMaxSeen) mockServoMaxSeen = angle;
}
void mockNeoPixelSet(int index, uint32_t color) {
  if (index < 0 || index >= 8) { mockInvalidPixelWrites++; return; }
  mockPixel[index] = color;
}

// ── Ghi lại output Serial ──
static std::string sSerialOut;
static std::string sSerialIn;

void mockSerialWriteStr(const char *s) { if (s) sSerialOut += s; }
void mockSerialPrintLong(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); sSerialOut += b; }
void mockSerialPrintFloat(double v, int digits) { char b[40]; snprintf(b, sizeof(b), "%.*f", digits, v); sSerialOut += b; }
void mockSerialPrintOther() { sSerialOut += "<obj>"; }

int SerialStub::available() { return (int)sSerialIn.size(); }
int SerialStub::read() {
  if (sSerialIn.empty()) return -1;
  int c = (unsigned char)sSerialIn.front();
  sSerialIn.erase(0, 1);
  return c;
}
void SerialStub::printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  sSerialOut += buf;
}

void mockSerialPush(const char *s) { if (s) sSerialIn += s; }
const char *mockSerialOutput() { return sSerialOut.c_str(); }
int mockSerialPending() { return (int)sSerialIn.size(); }
void mockSerialClear() { sSerialOut.clear(); sSerialIn.clear(); }

// ── mock WebServer ──
struct Route { std::string path; void (*fn)(); };
static Route sRoutes[24];
static int sRouteCount = 0;

struct Arg { std::string name; std::string value; };
static Arg sArgs[16];
static int sArgCount = 0;

// Header của request giả (test tự đặt trước khi gọi route — dùng để thử CSRF/token)
struct Header { std::string name; std::string value; };
static Header sHeaders[8];
static int sHeaderCount = 0;

// Danh sách header sketch đăng ký qua collectHeaders() — mô phỏng đúng WebServer
// thật: header không đăng ký thì handler không đọc được (hasHeader luôn false).
static std::string sCollected[8];
static int sCollectedCount = 0;

static std::string sLastType, sLastBody;
static int sLastCode = 0;

void WebServer::on(const char *path, void (*handler)()) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == path) { sRoutes[i].fn = handler; return; }
  }
  if (sRouteCount < 24) {
    sRoutes[sRouteCount].path = path;
    sRoutes[sRouteCount].fn = handler;
    sRouteCount++;
  }
}

void WebServer::collectHeaders(const char *keys[], size_t count) {
  sCollectedCount = 0;
  for (size_t i = 0; i < count && sCollectedCount < 8; i++) {
    sCollected[sCollectedCount++] = keys[i] ? keys[i] : "";
  }
}

static bool headerCollected(const char *name) {
  const char *n = name ? name : "";
  for (int i = 0; i < sCollectedCount; i++) {
    if (sCollected[i] == n) return true;
  }
  return false;
}

void WebServer::send(int code, const char *type, const char *body) {
  sLastCode = code; sLastType = type ? type : ""; sLastBody = body ? body : "";
}
void WebServer::send(int code, const char *type, const String &body) {
  sLastCode = code; sLastType = type ? type : ""; sLastBody = body.std_str();
}
void WebServer::send(int code) { sLastCode = code; sLastType.clear(); sLastBody.clear(); }
void WebServer::send_P(int code, const char *type, const char *body) { send(code, type, body); }
void WebServer::sendHeader(const char *name, const char *value) { (void)name; (void)value; }

String WebServer::arg(const char *name) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == name) return String(sArgs[i].value.c_str());
  }
  return String("");
}
bool WebServer::hasArg(const char *name) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == name) return true;
  }
  return false;
}

String WebServer::header(const char *name) {
  if (!headerCollected(name)) return String("");
  for (int i = 0; i < sHeaderCount; i++) {
    if (sHeaders[i].name == name) return String(sHeaders[i].value.c_str());
  }
  return String("");
}
bool WebServer::hasHeader(const char *name) {
  if (!headerCollected(name)) return false;
  for (int i = 0; i < sHeaderCount; i++) {
    if (sHeaders[i].name == name) return true;
  }
  return false;
}

void mockServerReset() {
  sRouteCount = 0;
  sArgCount = 0;
  sHeaderCount = 0;
  sCollectedCount = 0;   // "khởi động lại" → setup() sẽ collectHeaders lại
  sLastCode = 0; sLastType.clear(); sLastBody.clear();
}
int mockServerRouteCount() { return sRouteCount; }
bool mockServerHasRoute(const char *path) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == (path ? path : "")) return true;
  }
  return false;
}
void mockServerSetArg(const char *name, const char *value) {
  for (int i = 0; i < sArgCount; i++) {
    if (sArgs[i].name == (name ? name : "")) { sArgs[i].value = value ? value : ""; return; }
  }
  if (sArgCount < 16) {
    sArgs[sArgCount].name = name ? name : "";
    sArgs[sArgCount].value = value ? value : "";
    sArgCount++;
  }
}
void mockServerSetHeader(const char *name, const char *value) {
  for (int i = 0; i < sHeaderCount; i++) {
    if (sHeaders[i].name == (name ? name : "")) { sHeaders[i].value = value ? value : ""; return; }
  }
  if (sHeaderCount < 8) {
    sHeaders[sHeaderCount].name = name ? name : "";
    sHeaders[sHeaderCount].value = value ? value : "";
    sHeaderCount++;
  }
}
bool mockServerInvoke(const char *path) {
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == (path ? path : "")) {
      sLastCode = 0; sLastType.clear(); sLastBody.clear();
      sRoutes[i].fn();
      sArgCount = 0;
      sHeaderCount = 0;   // header chỉ có giá trị cho đúng một lần gọi
      return true;
    }
  }
  return false;
}
int mockServerLastCode() { return sLastCode; }
const char *mockServerLastType() { return sLastType.c_str(); }
const char *mockServerLastBody() { return sLastBody.c_str(); }

// ── mock OLED: một luồng + theo từng dòng text (setCursor y/8) ──
static std::string sOledText;
static std::string sOledLines[8];
static int sOledRow = 0;

static void oledAppend(const std::string &s) {
  sOledText += s;
  sOledLines[sOledRow] += s;
}

bool Adafruit_SSD1306::begin(int, int) { return mockDisplayBeginOk; }

void Adafruit_SSD1306::clearDisplay() {
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
}
void Adafruit_SSD1306::setCursor(int, int y) {
  sOledRow = y / 8;
  if (sOledRow < 0) sOledRow = 0;
  if (sOledRow > 7) sOledRow = 7;
}
void Adafruit_SSD1306::print(const char *s) { oledAppend(s ? s : ""); }
void Adafruit_SSD1306::print(char c) { oledAppend(std::string(1, c)); }
void Adafruit_SSD1306::print(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); oledAppend(b); }
void Adafruit_SSD1306::print(float v, int digits) { char b[40]; snprintf(b, sizeof(b), "%.*f", digits, v); oledAppend(b); }
void Adafruit_SSD1306::print(const IPAddress &ip) { oledAppend(ip.toString().c_str()); }
const char *mockOledText() { return sOledText.c_str(); }
const char *mockOledLine(int row) { return (row >= 0 && row < 8) ? sOledLines[row].c_str() : ""; }
int mockOledLineCount() { return 8; }

// ── mock Preferences ──
struct PrefEntry {
  char key[16];
  float value;
  bool used;
};
static PrefEntry sPrefs[24];

float mockPrefGet(const char *key, float dflt) {
  for (int i = 0; i < 24; i++) {
    if (sPrefs[i].used && strcmp(sPrefs[i].key, key) == 0) return sPrefs[i].value;
  }
  return dflt;
}

void mockPrefPut(const char *key, float value) {
  for (int i = 0; i < 24; i++) {
    if (sPrefs[i].used && strcmp(sPrefs[i].key, key) == 0) { sPrefs[i].value = value; return; }
  }
  for (int i = 0; i < 24; i++) {
    if (!sPrefs[i].used) {
      strncpy(sPrefs[i].key, key, sizeof(sPrefs[i].key) - 1);
      sPrefs[i].key[sizeof(sPrefs[i].key) - 1] = '\0';
      sPrefs[i].value = value;
      sPrefs[i].used = true;
      return;
    }
  }
}

void mockReset() {
  sMillis = 0;
  mockDhtTemp = 25.0f;
  mockDhtHum = 60.0f;
  memset(mockAdc, 0, sizeof(mockAdc));
  memset(mockDigital, 0, sizeof(mockDigital));
  memset(mockPinLevel, 0, sizeof(mockPinLevel));
  memset(mockPwm, 0, sizeof(mockPwm));
  memset(mockPixel, 0, sizeof(mockPixel));
  memset(sPrefs, 0, sizeof(sPrefs));
  mockServoAngle = 0;
  mockBuzzerToneHz = 0;
  mockWifiStatus = WL_CONNECTED;
  mockWifiIp = "192.168.1.50";
  mockDisplayBeginOk = true;

  mockDigitalWriteCount = 0;
  mockLedcWriteCount = 0;
  mockInvalidPixelWrites = 0;
  mockInvalidPwmWrites = 0;
  mockServoWriteCount = 0;
  mockServoMinSeen = 9999;
  mockServoMaxSeen = -1;
  mockToneCalls = mockNoToneCalls = 0;
  mockWifiBeginCalls = mockWifiDisconnectCalls = mockWifiReconnectCalls = 0;
  mockWdtInitCalls = mockWdtAddCalls = mockWdtResetCalls = 0;

  sSerialOut.clear();
  sSerialIn.clear();
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
  sIoLog.clear();
  mockServerReset();
}
