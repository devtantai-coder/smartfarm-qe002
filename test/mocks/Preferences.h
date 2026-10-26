#pragma once
#include "Arduino.h"
#include <map>
#include <string>

float mockPrefGet(const char *key, float dflt);
void  mockPrefPut(const char *key, float value);
const char *mockPrefGetStr(const char *key, const char *dflt);
void  mockPrefPutStr(const char *key, const char *value);
void  mockPrefClear();

class Preferences {
public:
  bool begin(const char *, bool) { return true; }
  void end() {}
  void putFloat(const char *key, float value) { mockPrefPut(key, value); }
  void putInt(const char *key, int value) { mockPrefPut(key, (float)value); }
  void putBool(const char *key, bool value) { mockPrefPut(key, value ? 1.0f : 0.0f); }
  void putString(const char *key, const char *value) { mockPrefPutStr(key, value); }
  void putString(const char *key, const String &value) { mockPrefPutStr(key, value.c_str()); }
  float getFloat(const char *key, float dflt) { return mockPrefGet(key, dflt); }
  int getInt(const char *key, int dflt) { return (int)mockPrefGet(key, (float)dflt); }
  bool getBool(const char *key, bool dflt) { return mockPrefGet(key, dflt ? 1.0f : 0.0f) != 0; }
  String getString(const char *key, const char *dflt) { return String(mockPrefGetStr(key, dflt)); }
  void clear() { mockPrefClear(); }
};
