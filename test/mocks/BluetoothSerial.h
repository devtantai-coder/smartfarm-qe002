#pragma once
#include <cstdint>
#include <cstring>
#include <deque>
#include <string>

// Mock BluetoothSerial cho test host: cho phép bơm byte vào như app gửi lệnh,
// đếm số lần firmware ghi phản hồi và reset trạng thái giữa các test.
class BluetoothSerial {
public:
    bool begin(const char* name = nullptr) { (void)name; return true; }
    bool begin(String, bool, bool) { return true; }
    bool begin(unsigned long) { return true; }
    void end() {}
    int available() { return (int)sIn.size(); }
    int read() {
        if (sIn.empty()) return -1;
        const unsigned char c = (unsigned char)sIn.front();
        sIn.pop_front();
        return (int)c;
    }
    String readStringUntil(char) { return String(""); }
    size_t write(uint8_t c) { (void)c; sWrites++; return 1; }
    size_t write(const uint8_t*, size_t size) { sWrites += (int)size; return size; }
    template<typename T> size_t println(T) { sWrites++; return 1; }
    template<typename T> size_t print(T) { sWrites++; return 1; }
    size_t println() { sWrites++; return 1; }
    template<typename... Args> size_t printf(const char*, Args...) { sWrites++; return 1; }
    void flush() {}
    bool connected() { return true; }
    operator bool() { return true; }

    void mockFeed(const char* text) {
        if (!text) return;
        for (const char* p = text; *p; p++) sIn.push_back(*p);
    }
    void mockFeed(const std::string& text) {
        for (char c : text) sIn.push_back(c);
    }
    void mockReset() { sIn.clear(); sWrites = 0; }
    int mockWriteCount() const { return sWrites; }

private:
    std::deque<char> sIn;
    int sWrites = 0;
};

extern BluetoothSerial SerialBT;
