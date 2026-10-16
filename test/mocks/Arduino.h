#pragma once
// Host-side simulation of the Arduino core API used by this project.
// Plain PC build: no ESP32 toolchain needed. See test/README.md.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <math.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define PROGMEM
#define F(x) x
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

class String {
public:
  String() {}
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(int v) { char b[16]; snprintf(b, sizeof(b), "%d", v); s_ = b; }
  String(long v) { char b[24]; snprintf(b, sizeof(b), "%ld", v); s_ = b; }
  String(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); s_ = b; }
  String(unsigned long v) { char b[24]; snprintf(b, sizeof(b), "%lu", v); s_ = b; }
  String(float v, int digits = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", digits, (double)v); s_ = b; }
  String(double v, int digits = 2) { char b[32]; snprintf(b, sizeof(b), "%.*f", digits, v); s_ = b; }
  void reserve(unsigned n) { s_.reserve(n); }
  int toInt() const { return atoi(s_.c_str()); }
  float toFloat() const { return (float)atof(s_.c_str()); }
  const char *c_str() const { return s_.c_str(); }
  unsigned length() const { return (unsigned)s_.size(); }
  char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  String &operator+=(const char *o) { s_ += (o ? o : ""); return *this; }
  String &operator+=(const String &o) { s_ += o.s_; return *this; }
  bool operator==(const char *o) const { return s_ == (o ? o : ""); }
  bool operator==(const String &o) const { return s_ == o.s_; }
  const std::string &std_str() const { return s_; }
private:
  std::string s_;
};
inline String operator+(const String &a, const char *b) { return String(a.std_str() + (b ? b : "")); }
inline String operator+(const char *a, const String &b) { return String(std::string(a ? a : "") + b.std_str()); }
inline String operator+(const String &a, const String &b) { return String(a.std_str() + b.std_str()); }
inline String operator+(const String &a, int b) { return String(a.std_str() + String(b).std_str()); }

unsigned long millis();
void delay(unsigned long);
void pinMode(uint8_t, uint8_t);
void digitalWrite(uint8_t, uint8_t);
int digitalRead(uint8_t);
int analogRead(uint8_t);
void ledcSetup(uint8_t, double, uint8_t);
void ledcAttachPin(uint8_t, uint8_t);
void ledcWrite(uint8_t, uint32_t);
void ledcAttach(uint8_t, uint32_t, uint8_t);
void tone(uint8_t, unsigned int, unsigned long = 0);
void noTone(uint8_t);

// Serial capture hooks (implemented in mock.cpp)
void mockSerialWriteStr(const char *s);
void mockSerialPrintLong(long v);
void mockSerialPrintFloat(double v, int digits);
void mockSerialPrintOther();

struct SerialStub {
  void begin(unsigned long) {}
  int available();
  int read();
  void print(char c) { char b[2] = {c, 0}; mockSerialWriteStr(b); }
  void print(const char *s) { mockSerialWriteStr(s); }
  void print(const String &s) { mockSerialWriteStr(s.c_str()); }
  void print(int v) { mockSerialPrintLong(v); }
  void print(unsigned int v) { mockSerialPrintLong((long)v); }
  void print(long v) { mockSerialPrintLong(v); }
  void print(unsigned long v) { mockSerialPrintLong((long)v); }
  void print(float v, int digits = 2) { mockSerialPrintFloat(v, digits); }
  void print(double v, int digits = 2) { mockSerialPrintFloat(v, digits); }
  template <typename T> void print(T) { mockSerialPrintOther(); }
  void println() { mockSerialWriteStr("\n"); }
  void println(char c) { print(c); mockSerialWriteStr("\n"); }
  void println(const char *s) { print(s); mockSerialWriteStr("\n"); }
  void println(const String &s) { print(s); mockSerialWriteStr("\n"); }
  void println(int v) { print(v); mockSerialWriteStr("\n"); }
  void println(unsigned int v) { print(v); mockSerialWriteStr("\n"); }
  void println(long v) { print(v); mockSerialWriteStr("\n"); }
  void println(unsigned long v) { print(v); mockSerialWriteStr("\n"); }
  void println(float v, int digits = 2) { print(v, digits); mockSerialWriteStr("\n"); }
  void println(double v, int digits = 2) { print(v, digits); mockSerialWriteStr("\n"); }
  template <typename T> void println(T) { mockSerialPrintOther(); mockSerialWriteStr("\n"); }
  void printf(const char *fmt, ...);
};
extern SerialStub Serial;

struct TwoWire {
  void begin(int, int) {}
};
extern TwoWire Wire;
