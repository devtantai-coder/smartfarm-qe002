#pragma once
#include "Arduino.h"

#define NEO_GRB 0x52
#define NEO_KHZ800 0x0000

void mockNeoPixelSet(int index, uint32_t color);

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(int, int, int) {}
  void begin() {}
  void show() {}
  void setPixelColor(int index, uint32_t color) { mockNeoPixelSet(index, color); }
  uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
  }
};
