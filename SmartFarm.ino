/*
 * SmartFarm.ino - QE002 ACEBOTT IoT Smart Farm
 * Entry point: setup() + loop()
 * Non-blocking, watchdog-protected, modular debug
 * Compatible with esp32 Arduino core 2.x and 3.x
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

#include "src/config.h"
#include "src/globals.h"
#include "src/logger.h"
#include "src/sensors.h"
#include "src/actuators.h"
#include "src/control.h"
#include "src/settings.h"
#include "src/oled.h"
#include "src/wifi.h"
#include "src/webserver.h"
#include "src/demo.h"

// ── Debug state ──
static char sSerialCmd = 0;
static bool sDebugMode = false;

// ── Watchdog timeout (ms) ──
static constexpr uint32_t WDT_TIMEOUT_MS = 30000;

static void handleFavicon() {
  server.send(204, "text/plain", "");
}

static void printDebugMenu() {
  Serial.println(F("\n=== SMART FARM DEBUG MENU ==="));
  Serial.println(F(" 1:Sensors  2:Actuators  3:Control"));
  Serial.println(F(" 4:Web      5:WiFi        6:OLED"));
  Serial.println(F(" 7:Settings 8:Logger      9:Demo"));
  Serial.println(F(" 0:Exit"));
  Serial.print(F("Chon: "));
}

static void handleDebugCmd(char cmd) {
  switch (cmd) {
    case '1': debugSensors(); break;
    case '2': debugActuators(); break;
    case '3': debugControl(); break;
    case '4': debugWebserver(); break;
    case '5': debugWifi(); break;
    case '6': debugOled(); break;
    case '7': debugSettings(); break;
    case '8': debugLogger(); break;
    case '0': sDebugMode = false; Serial.println(F(">> Exit debug.")); break;
    default:  Serial.println(F(">> Invalid.")); break;
  }
  if (cmd != '0') printDebugMenu();
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Watchdog (core 2.x / 3.x compatible init)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms = WDT_TIMEOUT_MS;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic = true;
  esp_task_wdt_deinit();
  esp_task_wdt_init(&wdtCfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_MS / 1000, true);
#endif
  esp_task_wdt_add(NULL);

  LOG_INFO("Smart Farm starting...");

  // GPIO: set the output latch BEFORE pinMode so no glitch appears on boot
  // (an active-LOW relay would briefly switch on if driven LOW first).
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);  // pump OFF at boot
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  pinMode(PIR_PIN, INPUT);

  // PWM for blue LED: steady on/off + breathing effect (core 2.x / 3.x)
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(BLUE_LED_PIN, 1000, 8);
#else
  ledcSetup(LED_PWM_CHANNEL, 1000, 8);
  ledcAttachPin(BLUE_LED_PIN, LED_PWM_CHANNEL);
#endif
  setLedPwm(0);

  // Peripherals
  dht.begin();
  strip.begin();
  strip.show();
  servo.attach(SERVO_PIN);
  servo.write(servoAngle);

  // OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    LOG_ERROR_CODE(101, "OLED not found");
  }
  display.clearDisplay();
  display.display();

  // Load settings from flash
  loadSettings();
  LOG_INFO("Settings loaded");
  if (breatheAuto) setBreatheLed(true);

  // WiFi: short blocking wait (watchdog is fed), then manageWiFi() retries
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  LOG_INFO("Connecting WiFi...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    esp_task_wdt_reset();
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP: "));
    Serial.println(WiFi.localIP());
  } else {
    LOG_ERROR_CODE(102, "WiFi timeout - retrying in background");
  }

  // Web server routes
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.on("/toggle", handleToggle);
  server.on("/auto", handleAuto);
  server.on("/rgb", handleRgb);
  server.on("/servo", handleServo);
  server.on("/breathe", handleBreatheLed);
  server.on("/on", handleLedOn);
  server.on("/off", handleLedOff);
  server.on("/windmill", handleWindmill);
  server.on("/feed", handleFeed);
  server.on("/demo", handleDemo);
  server.on("/threshold", handleThreshold);
  server.on("/favicon.ico", handleFavicon);
  server.begin();

  LOG_INFO("System ready");
  printDebugMenu();
}

void loop() {
  esp_task_wdt_reset();
  server.handleClient();
  manageWiFi();

  // Serial debug input
  if (Serial.available() > 0) {
    sSerialCmd = Serial.read();
    if (sSerialCmd == '\n' || sSerialCmd == '\r') {
      // skip
    } else if (!sDebugMode) {
      if (sSerialCmd == '9') {
        startDemo();
      } else if (sSerialCmd >= '0' && sSerialCmd <= '8') {
        sDebugMode = true;
        handleDebugCmd(sSerialCmd);
      } else {
        Serial.println(F(">> Invalid. Enter 0-9."));
        printDebugMenu();
      }
    } else {
      if (sSerialCmd == '0') {
        handleDebugCmd('0');
        Serial.println(F("Running normally..."));
      } else {
        Serial.println(F(">> Enter '0' to exit debug."));
      }
    }
  }

  if (sDebugMode) return;

  unsigned long now = millis();

  // Fast digital inputs: touch/PIR must react faster than the 1s sensor cycle
  gTouch = readTouch();
  gPir = readPIR();

  // Read sensors periodically (DHT11 needs >= 1s between reads)
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    readAllSensors();
    logSensors();   // one line every second at DEBUG level (activity 7)
    lastSensorRead = now;
  }

  // Automation logic (paused while the activity demo is running)
  if (demoRunning()) {
    updateDemo();
    controlBreathe();   // keep the HD3 breathing step animating during the demo
  } else {
    controlPump();
    controlFan();
    controlLight();
    controlRgb();
    controlBreathe();
    controlTouch();
    controlAlerts();
    controlWindmill();
    controlFeed();
  }

  // Update OLED
  if (now - lastOledUpdate >= OLED_INTERVAL) {
    updateOLED(gTemp, gHum, gLight, gSoil, gWater);
    lastOledUpdate = now;
  }
}
