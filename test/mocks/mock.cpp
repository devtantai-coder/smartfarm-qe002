#include "mock.h"
#include "Arduino.h"
#include "DHT.h"
#include "ESP32Servo.h"
#include "Adafruit_NeoPixel.h"
#include "Adafruit_SSD1306.h"
#include "WebServer.h"
#include "Wire.h"
#include "WiFi.h"
#include "BluetoothSerial.h"
#include "esp_task_wdt.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

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
int mockWifiStatusCalls = 0;
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
int mockWifiPersistentCalls = 0, mockWifiSleepDisableCalls = 0;
int mockI2cCmdBytes = 0, mockI2cDataBytes = 0, mockI2cTransactions = 0;
int mockI2cFailCount = 0;
int mockI2cTxBytes = 0;
int mockOtaHandleCalls = 0;
int mockStripShowCount = 0;
int mockWdtInitCalls = 0, mockWdtAddCalls = 0, mockWdtResetCalls = 0;

TwoWire Wire;
WiFiStub WiFi;
SerialStub Serial;

// ── Lõi Arduino ──
unsigned long millis() { return sMillis; }
unsigned long micros() { return sMillis * 1000UL; }
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

static uint8_t mockPinModeAt[40] = {0};
void pinMode(uint8_t pin, uint8_t mode) {
  if (pin < 40) mockPinModeAt[pin] = mode;
  ioLogAppend('M', pin, 0);
}
// Chế độ đã cấu hình cho một chân (để test khẳng định INPUT_PULLDOWN...).
int mockGetPinMode(uint8_t pin) { return pin < 40 ? mockPinModeAt[pin] : -1; }
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
void setToneChannel(uint8_t) {}

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
// method được ghi lại để test kiểm tra được BẢNG route: đăng ký thiếu method
// trong core thật là HTTP_ANY (nhận cả POST → body đọc trên loopTask).
struct Route { std::string path; void (*fn)(); int method; };
// Trước đây mảng chỉ có 24 chỗ trong khi firmware đăng ký đúng 24 route: thêm
// một route là bị âm thầm bỏ qua và test vẫn xanh.Để dư + có cờ tràn.
static const int MOCK_MAX_ROUTES = 40;
static Route sRoutes[MOCK_MAX_ROUTES];
static int sRouteCount = 0;
static bool sRouteOverflow = false;

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
// Content-Length handler khai báo qua setContentLength(); test so với độ dài
// body thật để bắt lỗi khai báo sai (trình duyệt sẽ cắt/treo response).
static size_t sContentLength = 0;
// Số lần sendContent() của handler (xem WebServer::sendContent).
static int sContentWrites = 0;
// Header "Host" của request giả; rỗng = dùng mockWifiIp (mở bằng IP).
static std::string sHost;

static void registerRoute(const char *path, int method, void (*handler)()) {
  const std::string p = path ? path : "";
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path == p) { sRoutes[i].fn = handler; sRoutes[i].method = method; return; }
  }
  if (sRouteCount < MOCK_MAX_ROUTES) {
    sRoutes[sRouteCount].path = p;
    sRoutes[sRouteCount].fn = handler;
    sRoutes[sRouteCount].method = method;
    sRouteCount++;
  } else {
    sRouteOverflow = true;   // test sẽ báo nếu firmware đăng ký vượt sức chứa mock
  }
}

// Quá tải: core ánh xạ overload này thành HTTP_ANY, KHÔNG phải HTTP_GET.
void WebServer::on(const char *path, void (*handler)()) {
  registerRoute(path, HTTP_ANY, handler);
}

void WebServer::on(const char *path, HTTPMethod method, void (*handler)()) {
  registerRoute(path, method, handler);
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
// Ghi body thô kiểu send(code,type,"") + setContentLength + sendContent:
// send() xoá body cũ, sendContent() nối phần thân vào sau.
void WebServer::setContentLength(size_t length) { sContentLength = length; }
void WebServer::sendContent(const char *content, size_t length) {
  // Mỗi sendContent() là MỘT NetworkClient::write() trên firmware thật, và mỗi
  // write có thể chặn tới 10 s. Đếm số lần ghi để test bắt được handler stream
  // body thành hàng chục khúc (đó là cách loopTask bị giữ quá mức 30 s của WDT).
  sContentWrites++;
  if (content && length) sLastBody.append(content, length);
}
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

// Host client đã dùng để gọi tới. Mô phỏng đúng cả hai kiểu truy cập:
// bằng IP (mặc định) và bằng tên mDNS (mockServerSetHost("smartfarm.local")).
String WebServer::hostHeader() {
  return String(sHost.empty() ? mockWifiIp : sHost.c_str());
}
void mockServerSetHost(const char *host) { sHost = host ? host : ""; }

void mockServerReset() {
  sRouteCount = 0;
  sRouteOverflow = false;
  sArgCount = 0;
  sHeaderCount = 0;
  sCollectedCount = 0;   // "khởi động lại" → setup() sẽ collectHeaders lại
  sContentLength = 0;
  sContentWrites = 0;
  sHost.clear();
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
bool mockServerInvokeMethod(const char *path, int method) {
  const std::string p = path ? path : "";
  bool pathExists = false;
  for (int i = 0; i < sRouteCount; i++) {
    if (sRoutes[i].path != p) continue;
    pathExists = true;
    // canHandle của core: _method != HTTP_ANY && _method != requestMethod -> skip
    if (sRoutes[i].method != HTTP_ANY && sRoutes[i].method != method) continue;
    sLastCode = 0; sLastType.clear(); sLastBody.clear(); sContentLength = 0;
    sContentWrites = 0;
    sRoutes[i].fn();
    sArgCount = 0;
    sHeaderCount = 0;   // header chỉ có giá trị cho đúng một lần gọi
    return true;
  }
  if (pathExists) sLastCode = 404;   // không handler nào khớp method -> core trả 404
  return false;
}

bool mockServerInvoke(const char *path) {
  return mockServerInvokeMethod(path, HTTP_GET);
}

int mockServerRouteMethod(int index) {
  if (index < 0 || index >= sRouteCount) return -1;
  return sRoutes[index].method;
}
const char *mockServerRoutePath(int index) {
  if (index < 0 || index >= sRouteCount) return "";
  return sRoutes[index].path.c_str();
}
bool mockServerRouteOverflow() { return sRouteOverflow; }
int mockServerLastCode() { return sLastCode; }
const char *mockServerLastType() { return sLastType.c_str(); }
const char *mockServerLastBody() { return sLastBody.c_str(); }
size_t mockServerContentLength() { return sContentLength; }
int mockServerContentWrites() { return sContentWrites; }

// ── mock OLED: một luồng + theo từng dòng text (setCursor y/8) ──
static std::string sOledText;
static std::string sOledLines[8];
static int sOledRow = 0;
// Framebuffer giả 1024 byte mô phỏng đúng cách sắp xếp pixel của font 6x8:
// mỗi ký tự chiếm 6 cột (cột 6i..6i+5) trong page 8-hàng tương ứng. Nhờ vậy
// khi đổi 1 ký tự thì chỉ 6 cột của page đó đổi — giống phần cứng thật — để
// updateOLED() diff theo dải cột được kiểm chứng đúng.
static uint8_t sOledBuf[128 * 64 / 8];

static void oledSyncBuffer() {
  memset(sOledBuf, 0, sizeof(sOledBuf));
  for (int row = 0; row < 8; row++) {
    const std::string &line = sOledLines[row];
    for (size_t i = 0; i < line.size(); i++) {
      // Chữ ký riêng cho từng ký tự: đổi ký tự này chỉ đổi 6 cột của nó.
      uint8_t sig = (uint8_t)((uint32_t)line[i] * 31u + (uint32_t)i * 13u + 7u);
      for (int k = 0; k < 6; k++) {
        size_t col = i * 6 + k;
        if (col >= 128) break;
        sOledBuf[row * 128 + col] = sig;
      }
    }
  }
}

static void oledAppend(const std::string &s) {
  sOledText += s;
  sOledLines[sOledRow] += s;
  oledSyncBuffer();
}

bool Adafruit_SSD1306::begin(int, int) { return mockDisplayBeginOk; }

uint8_t *Adafruit_SSD1306::getBuffer() { return sOledBuf; }

void Adafruit_SSD1306::clearDisplay() {
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
  oledSyncBuffer();
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
// Mỗi lần put* là một lần nvs_commit() — tức một lượt mòn flash. Test dùng số
// này để bắt các hàm "lưu" ghi lại y nguyên giá trị cũ.
static int sPrefWrites = 0;
int mockPrefWrites() { return sPrefWrites; }

float mockPrefGet(const char *key, float dflt) {
  for (int i = 0; i < 24; i++) {
    if (sPrefs[i].used && strcmp(sPrefs[i].key, key) == 0) return sPrefs[i].value;
  }
  return dflt;
}

void mockPrefPut(const char *key, float value) {
  sPrefWrites++;
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

// ── mock Preferences (string) ──
struct StrPrefEntry {
  char key[16];
  char value[64];
  bool used;
};
static StrPrefEntry sStrPrefs[16];

const char *mockPrefGetStr(const char *key, const char *dflt) {
  for (int i = 0; i < 16; i++) {
    if (sStrPrefs[i].used && strcmp(sStrPrefs[i].key, key) == 0) return sStrPrefs[i].value;
  }
  return dflt;
}

void mockPrefPutStr(const char *key, const char *value) {
  sPrefWrites++;
  for (int i = 0; i < 16; i++) {
    if (sStrPrefs[i].used && strcmp(sStrPrefs[i].key, key) == 0) {
      strncpy(sStrPrefs[i].value, value, sizeof(sStrPrefs[i].value) - 1);
      sStrPrefs[i].value[sizeof(sStrPrefs[i].value) - 1] = '\0';
      return;
    }
  }
  for (int i = 0; i < 16; i++) {
    if (!sStrPrefs[i].used) {
      strncpy(sStrPrefs[i].key, key, sizeof(sStrPrefs[i].key) - 1);
      sStrPrefs[i].key[sizeof(sStrPrefs[i].key) - 1] = '\0';
      strncpy(sStrPrefs[i].value, value, sizeof(sStrPrefs[i].value) - 1);
      sStrPrefs[i].value[sizeof(sStrPrefs[i].value) - 1] = '\0';
      sStrPrefs[i].used = true;
      return;
    }
  }
}

void mockPrefClear() {
  for (int i = 0; i < 24; i++) sPrefs[i].used = false;
  for (int i = 0; i < 16; i++) sStrPrefs[i].used = false;
}

void mockReset() {
  sMillis = 0;
  mockDhtTemp = 25.0f;
  mockDhtHum = 60.0f;
  memset(mockAdc, 0, sizeof(mockAdc));
  memset(mockDigital, 0, sizeof(mockDigital));
  memset(mockPinLevel, 0, sizeof(mockPinLevel));
  memset(mockPinModeAt, 0, sizeof(mockPinModeAt));
  memset(mockPwm, 0, sizeof(mockPwm));
  memset(mockPixel, 0, sizeof(mockPixel));
  memset(sPrefs, 0, sizeof(sPrefs));
  sPrefWrites = 0;
  mockServoAngle = 0;
  mockBuzzerToneHz = 0;
  mockWifiStatus = WL_CONNECTED;
  mockWifiStatusCalls = 0;
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
  mockWifiPersistentCalls = mockWifiSleepDisableCalls = 0;
  mockI2cCmdBytes = mockI2cDataBytes = mockI2cTransactions = 0;
  mockI2cFailCount = 0;
  mockI2cTxBytes = 0;
  mockOtaHandleCalls = 0;
  mockStripShowCount = 0;
  mockWdtInitCalls = mockWdtAddCalls = mockWdtResetCalls = 0;

  sSerialOut.clear();
  sSerialIn.clear();
  sOledText.clear();
  for (int i = 0; i < 8; i++) sOledLines[i].clear();
  sOledRow = 0;
  sIoLog.clear();
  mockServerReset();
}

// ── BluetoothSerial ──
BluetoothSerial SerialBT;

// ── Mock FreeRTOS ──
#include <vector>
#include <cstring>

struct MockQueue {
  size_t len, itemSize, head, tail, count;
  std::vector<unsigned char> buf;
};
struct MockEventGroup { uint32_t bits; };

extern "C" BaseType_t xTaskCreate(TaskFunction_t, const char*, uint32_t, void*, UBaseType_t, TaskHandle_t*) { return pdPASS; }
extern "C" void vTaskDelay(TickType_t ticksToDelay) { sMillis += (unsigned long)ticksToDelay; }
extern "C" TickType_t xTaskGetTickCount(void) { return (TickType_t)millis(); }
extern "C" void vTaskDelete(TaskHandle_t) {}

extern "C" QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize) {
  MockQueue *q = new MockQueue();
  q->len = uxQueueLength ? uxQueueLength : 1;
  q->itemSize = uxItemSize ? uxItemSize : 1;
  q->head = q->tail = q->count = 0;
  q->buf.assign(q->len * q->itemSize, 0);
  return (QueueHandle_t)q;
}
extern "C" BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t) {
  MockQueue *q = (MockQueue*)xQueue;
  if (!q) return pdFAIL;
  if (q->count >= q->len) {
    // drop oldest
    q->head = (q->head + 1) % q->len;
    q->count--;
  }
  memcpy(q->buf.data() + q->tail * q->itemSize, pvItemToQueue, q->itemSize);
  q->tail = (q->tail + 1) % q->len;
  q->count++;
  return pdPASS;
}
extern "C" BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t) {
  MockQueue *q = (MockQueue*)xQueue;
  if (!q || q->count == 0) return pdFALSE;
  memcpy(pvBuffer, q->buf.data() + q->head * q->itemSize, q->itemSize);
  q->head = (q->head + 1) % q->len;
  q->count--;
  return pdTRUE;
}
extern "C" UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue) {
  MockQueue *q = (MockQueue*)xQueue;
  return q ? (UBaseType_t)q->count : 0;
}

extern "C" SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)new int(1); }
extern "C" SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return (SemaphoreHandle_t)new int(1); }
extern "C" SemaphoreHandle_t xSemaphoreCreateBinary(void) { return (SemaphoreHandle_t)new int(0); }
extern "C" BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
extern "C" BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
extern "C" BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
extern "C" BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t) { return pdTRUE; }

extern "C" EventGroupHandle_t xEventGroupCreate(void) { return (EventGroupHandle_t)new MockEventGroup{0}; }
extern "C" EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet) {
  MockEventGroup *eg = (MockEventGroup*)xEventGroup;
  if (!eg) return 0;
  eg->bits |= uxBitsToSet;
  return eg->bits;
}
extern "C" EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear) {
  MockEventGroup *eg = (MockEventGroup*)xEventGroup;
  if (!eg) return 0;
  eg->bits &= ~uxBitsToClear;
  return eg->bits;
}
extern "C" EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor,
                                          BaseType_t, BaseType_t, TickType_t) {
  MockEventGroup *eg = (MockEventGroup*)xEventGroup;
  return eg ? eg->bits & uxBitsToWaitFor : 0;
}
extern "C" EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup) {
  MockEventGroup *eg = (MockEventGroup*)xEventGroup;
  return eg ? eg->bits : 0;
}
