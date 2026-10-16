#pragma once
#include "Arduino.h"
#include "Wire.h"
#include "WiFi.h"

#define SSD1306_SWITCHCAPVCC 2
#define SSD1306_WHITE 1

// OLED mock: captures everything printed, both as one stream and per text row
// (setCursor y selects the row, y/8). See mock.cpp/mock.h.
class Adafruit_SSD1306 {
public:
  Adafruit_SSD1306(int, int, TwoWire *, int) {}
  bool begin(int, int);
  void clearDisplay();
  void display() {}
  void setTextColor(int) {}
  void setCursor(int x, int y);
  void setTextSize(int) {}
  void print(const char *s);
  void print(char c);
  void print(int v);
  void print(float v, int digits);
  void print(const IPAddress &ip);
  template <typename T> void print(T) { print("<obj>"); }
};
