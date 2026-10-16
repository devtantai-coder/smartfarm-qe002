#pragma once
// API mock phần cứng điều khiển được cho bộ test chạy trên host.
// Các test link firmware THẬT (SmartFarm.ino + src/*.cpp) với các
// mock này, nên mọi kiểm tra đều chạy trên code production. Xem test/README.md.
#include <cstdint>

// ── Trạng thái phần cứng giả lập ──
extern float mockDhtTemp, mockDhtHum;   // giá trị đọc từ DHT11
extern int mockAdc[40];                 // giá trị analogRead theo chân
extern uint8_t mockDigital[40];         // giá trị digitalRead theo chân
extern uint32_t mockPinLevel[40];       // mức digitalWrite ghi lại theo chân
extern int mockServoAngle;              // góc servo.write() lần cuối
extern uint32_t mockPwm[40];            // giá trị ledcWrite() lần cuối theo kênh/chân
extern int mockBuzzerToneHz;            // tần số tone(), 0 = im lặng
extern uint32_t mockPixel[8];           // màu NeoPixel đã ghi
extern int mockWifiStatus;              // kết quả WiFi.status()
extern const char *mockWifiIp;          // chuỗi WiFi.localIP()
extern bool mockDisplayBeginOk;         // kết quả display.begin() (false = thiếu OLED)

// ── Bộ đếm instrumentation (test chuyên sâu) ──
extern int mockDigitalWriteCount;       // số lần gọi digitalWrite()
extern int mockLedcWriteCount;          // số lần gọi ledcWrite()
extern int mockInvalidPixelWrites;      // setPixelColor với index ngoài phạm vi
extern int mockInvalidPwmWrites;        // ledcWrite với kênh/chân ngoài phạm vi
extern int mockServoWriteCount;         // số lần gọi servo.write()
extern int mockServoMinSeen;            // góc nhỏ nhất từng ghi (9999 nếu chưa có)
extern int mockServoMaxSeen;            // góc lớn nhất từng ghi (-1 nếu chưa có)
extern int mockToneCalls, mockNoToneCalls;
extern int mockWifiBeginCalls, mockWifiDisconnectCalls, mockWifiReconnectCalls;
extern int mockWdtInitCalls, mockWdtAddCalls, mockWdtResetCalls;

unsigned long mockMillis();
void mockAdvance(unsigned long ms);
void mockSetMillis(unsigned long ms);
void mockReset();

// Serial
void mockSerialPush(const char *s);
const char *mockSerialOutput();
int mockSerialPending();
void mockSerialClear();

// Log thứ tự gọi IO cho test regression lỗi glitch lúc khởi động:
// mỗi mục có dạng "W:26=1;" (digitalWrite) và "M:26;" (pinMode).
const char *mockIoLog();
void mockIoLogClear();

// WebServer
void mockServerReset();
int  mockServerRouteCount();
bool mockServerHasRoute(const char *path);
void mockServerSetArg(const char *name, const char *value);
void mockServerSetHeader(const char *name, const char *value);
bool mockServerInvoke(const char *path);
int  mockServerLastCode();
const char *mockServerLastType();
const char *mockServerLastBody();

// OLED
const char *mockOledText();
const char *mockOledLine(int row);      // text ghi lại cho mỗi dòng text 8-px
int mockOledLineCount();

// Các hook hiện thực trong mock.cpp, được gọi từ các header mô phỏng
void mockServoWrite(int angle);
void mockNeoPixelSet(int index, uint32_t color);
float mockPrefGet(const char *key, float dflt);
void mockPrefPut(const char *key, float value);
