#include "globals.h"

// ── Trạng thái ──
bool pumpAuto = true, lightAuto = true, rgbAuto = true, fanAuto = true, breatheAuto = false;
bool relayState = false, fanState = false, blueLedState = false, breatheLedState = false;
bool windmillOn = false;
uint8_t rgbR = 0, rgbG = 0, rgbB = 0;
uint8_t breatheBrightness = 0;
int8_t breatheDirection = 1;
int servoAngle = 0;

// ── Cảm biến ──
float gTemp = 0, gHum = 0;
int gLight = 0, gSoil = 0, gWater = 0;
bool gPir = false, gTouch = false;

// ── Thời gian ──
unsigned long lastWifiCheck = 0, lastSensorRead = 0, lastOledUpdate = 0;
unsigned long lastBuzzer = 0, lastTouch = 0, lastBreatheUpdate = 0;

// ── Phần cứng ──
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel strip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);
Servo servo;
WebServer server(80);
Preferences preferences;

// ── Sức khỏe ──
uint8_t sensorFailCount = 0;
bool sensorHealthy = true;

// ── Chống replay ──
uint32_t gLastNonce = 0;
