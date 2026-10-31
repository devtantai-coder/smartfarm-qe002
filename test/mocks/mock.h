#pragma once
// API mock phần cứng điều khiển được cho bộ test chạy trên host.
// Các test link firmware THẬT (SmartFarm.ino + src/*.cpp) với các
// mock này, nên mọi kiểm tra đều chạy trên code production. Xem test/README.md.
#include <cstddef>
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
extern int mockI2cCmdBytes, mockI2cDataBytes, mockI2cTransactions;
extern int mockI2cFailCount;        // số lần endTransmission() trả lỗi (bus I2C hỏng)
extern int mockStripShowCount;          // số lần strip.show() (giao dịch RMT)

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
// Chế độ pinMode() cuối cùng của một chân (so sánh với INPUT/INPUT_PULLDOWN/...).
int mockGetPinMode(uint8_t pin);
void mockIoLogClear();

// WebServer
void mockServerReset();
int  mockServerRouteCount();
bool mockServerHasRoute(const char *path);
void mockServerSetArg(const char *name, const char *value);
void mockServerSetHeader(const char *name, const char *value);
// Header "Host" của request giả (mặc định = mockWifiIp, giống khi mở bằng IP).
void mockServerSetHost(const char *host);
bool mockServerInvoke(const char *path);
// Gọi route với method cụ thể (HTTP_GET/HTTP_POST/... trong mocks/WebServer.h).
// Trả false nếu không có route nào nhận method đó; khi đường dẫn có tồn tại
// nhưng method không khớp, mã trả về là 404 — đúng như core thật.
bool mockServerInvokeMethod(const char *path, int method);
// Bảng route đã đăng ký (để test Assert mọi route state-changing là GET tường minh).
int mockServerRouteMethod(int index);
const char *mockServerRoutePath(int index);
// true nếu firmware đăng ký nhiều route hơn sức chứa mock (trước đây bị âm thầm bỏ qua).
bool mockServerRouteOverflow();
int  mockServerLastCode();
const char *mockServerLastType();
const char *mockServerLastBody();
// Số byte mà handler khai báo qua setContentLength() (0 = không khai báo).
size_t mockServerContentLength();
// Số lần handler gọi sendContent() cho response vừa rồi. Mỗi lần là một
// NetworkClient::write() chặn được tới 10 s trên firmware thật, nên response
// state-changing nào cũng phải ghi đúng MỘT lần.
int mockServerContentWrites();

// OLED
const char *mockOledText();
const char *mockOledLine(int row);      // text ghi lại cho mỗi dòng text 8-px
int mockOledLineCount();

// Các hook hiện thực trong mock.cpp, được gọi từ các header mô phỏng
void mockServoWrite(int angle);
void mockNeoPixelSet(int index, uint32_t color);
float mockPrefGet(const char *key, float dflt);
void mockPrefPut(const char *key, float value);
// Số lần put* (= số lần commit flash NVS) kể từ mockReset() gần nhất.
int mockPrefWrites();
