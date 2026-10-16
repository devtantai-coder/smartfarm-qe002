#pragma once
// Controllable hardware mock API for the host-side test suite.
// The tests link the REAL firmware (SmartFarm.ino + src/*.cpp) against these
// mocks, so every check exercises production code. See test/README.md.
#include <cstdint>

// ── Fake hardware state ──
extern float mockDhtTemp, mockDhtHum;   // DHT11 readings
extern int mockAdc[40];                 // analogRead values by pin
extern uint8_t mockDigital[40];         // digitalRead values by pin
extern uint32_t mockPinLevel[40];       // digitalWrite levels captured by pin
extern int mockServoAngle;              // last servo.write() angle
extern uint32_t mockPwm[40];            // last ledcWrite() value by channel/pin
extern int mockBuzzerToneHz;            // tone() frequency, 0 = silent
extern uint32_t mockPixel[8];           // NeoPixel colors written
extern int mockWifiStatus;              // WiFi.status() result
extern const char *mockWifiIp;          // WiFi.localIP() string
extern bool mockDisplayBeginOk;         // display.begin() result (false = missing OLED)

// ── Instrumentation counters (deep tests) ──
extern int mockDigitalWriteCount;       // digitalWrite() calls
extern int mockLedcWriteCount;          // ledcWrite() calls
extern int mockInvalidPixelWrites;      // setPixelColor with out-of-range index
extern int mockInvalidPwmWrites;        // ledcWrite with out-of-range channel/pin
extern int mockServoWriteCount;         // servo.write() calls
extern int mockServoMinSeen;            // min angle ever written (9999 if none)
extern int mockServoMaxSeen;            // max angle ever written (-1 if none)
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

// IO call-order log for the boot-glitch regression test:
// entries look like "W:26=1;" (digitalWrite) and "M:26;" (pinMode).
const char *mockIoLog();
void mockIoLogClear();

// WebServer
void mockServerReset();
int  mockServerRouteCount();
bool mockServerHasRoute(const char *path);
void mockServerSetArg(const char *name, const char *value);
bool mockServerInvoke(const char *path);
int  mockServerLastCode();
const char *mockServerLastType();
const char *mockServerLastBody();

// OLED
const char *mockOledText();
const char *mockOledLine(int row);      // captured text per 8-px text row
int mockOledLineCount();

// Hooks implemented in mock.cpp, called from the simulation headers
void mockServoWrite(int angle);
void mockNeoPixelSet(int index, uint32_t color);
float mockPrefGet(const char *key, float dflt);
void mockPrefPut(const char *key, float value);
