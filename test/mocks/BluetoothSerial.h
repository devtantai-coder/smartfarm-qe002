#pragma once
#include <cstdint>
#include <cstring>

class BluetoothSerial {
public:
    bool begin(const char* name = nullptr) { return true; }
    bool begin(String, bool, bool) { return true; }
    bool begin(unsigned long) { return true; }
    void end() {}
    int available() { return 0; }
    String readStringUntil(char) { return String(""); }
    int read() { return -1; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t*, size_t size) { return size; }
    template<typename T> size_t println(T) { return 1; }
    template<typename T> size_t print(T) { return 1; }
    size_t println() { return 1; }
    template<typename... Args> size_t printf(const char*, Args...) { return 1; }
    void flush() {}
    bool connected() { return false; }
    operator bool() { return true; }
};

extern BluetoothSerial SerialBT;
