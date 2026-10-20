#include "globals.h"

// ── Trạng thái toàn cục ──
// Giá trị mặc định nằm ngay trong định nghĩa SystemState (globals.h);
// ở đây chỉ có đúng một định nghĩa duy nhất cho toàn chương trình.
SystemState sys;

// ── Phần cứng ──
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);
Servo servo;
WebServer server(80);
Preferences preferences;
