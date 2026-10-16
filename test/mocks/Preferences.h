#pragma once
#include "Arduino.h"

float mockPrefGet(const char *key, float dflt);
void  mockPrefPut(const char *key, float value);

class Preferences {
public:
  bool begin(const char *, bool) { return true; }
  void end() {}
  void putFloat(const char *key, float value) { mockPrefPut(key, value); }
  void putInt(const char *key, int value) { mockPrefPut(key, (float)value); }
  void putBool(const char *key, bool value) { mockPrefPut(key, value ? 1.0f : 0.0f); }
  float getFloat(const char *key, float dflt) { return mockPrefGet(key, dflt); }
  int getInt(const char *key, int dflt) { return (int)mockPrefGet(key, (float)dflt); }
  bool getBool(const char *key, bool dflt) { return mockPrefGet(key, dflt ? 1.0f : 0.0f) != 0; }
};
