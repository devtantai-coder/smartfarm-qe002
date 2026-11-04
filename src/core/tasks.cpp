#include "tasks.h"
#include "globals.h"
#include "logger.h"
#include "rtos.h"
#include "config.h"
#include "settings.h"
#include "calibration.h"
#include "anomaly.h"
#include "../sensors/sensors.h"
#include "../actuators/actuators.h"
#include "../control/control.h"
#include "../control/demo.h"
#include "../control/pid.h"
#include "../control/softstart.h"
#include "../connectivity/wifi.h"
#include "../connectivity/mqtt.h"
#include "../connectivity/websocket.h"
#include "../connectivity/ota.h"
#include "../connectivity/tls.h"
#include "../ui/oled.h"
#include "../observability/metrics.h"
#include "../observability/health.h"
#include "../hal/hal.h"
#include <ArduinoOTA.h>
#ifndef SMARTFARM_TEST_HOOKS
#include <esp_sleep.h>
#endif

extern void runAutomation();

void sensorTaskFn(void*) {
  for (;;) {
    // Sensor task là writer duy nhất cho gTemp/gHum/...; giữ mutex khi ghi
    // để control/display task không đọc nửa vời.
    SensorData sd;
    rtosLock();
    readAllSensors();
    sd.temp = sys.gTemp; sd.hum = sys.gHum;
    sd.light = sys.gLight; sd.soil = sys.gSoil; sd.water = sys.gWater;
    sd.pir = sys.gPir; sd.touch = sys.gTouch;
    rtosUnlock();
    // Đẩy snapshot qua queue để consumer (control) có thể log/monitor
    // mà không cần giữ mutex (xQueueOverwrite = latest-wins, không block).
    if (qSensorData) xQueueOverwrite(qSensorData, &sd);
    if (evtGroup) xEventGroupSetBits(evtGroup, EVT_SENSOR_READY);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}
void controlTaskFn(void*) {
  for (;;) {
    // Dọn queue sensor (latest) trước runAutomation — quan sát nhịp producer.
    SensorData sd;
    if (qSensorData) while (xQueueReceive(qSensorData, &sd, 0) == pdTRUE) {}
    if (evtGroup) xEventGroupClearBits(evtGroup, EVT_SENSOR_READY);
    runAutomation();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void commTaskFn(void*) {
  for (;;) {
    server.handleClient();
    manageWiFi();
    mqttLoop();
    websocketLoop();
    otaLoop();
    ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void displayTaskFn(void*) {
  for (;;) {
    unsigned long now = millis();
    bool need = false;
    DisplaySnapshot snap;
    // Chụp snapshot dưới mutex để không đọc torn float/int giữa các task.
    // updateOLED (I2C chậm) được gọi NGAY NGOÀI lock.
    rtosLock();
    need = sys.oledDirty || (now - sys.lastOledUpdate >= OLED_INTERVAL);
    if (need) {
      snap.temp = sys.gTemp; snap.hum = sys.gHum;
      snap.light = sys.gLight; snap.soil = sys.gSoil; snap.water = sys.gWater;
      snap.relayState = sys.relayState; snap.fanState = sys.fanState;
      snap.windmillOn = sys.windmillOn;
      sys.lastOledUpdate = now;
      sys.oledDirty = false;
    }
    rtosUnlock();
    if (need) {
      updateOLED(snap.temp, snap.hum, snap.light, snap.soil, snap.water);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
void powerTaskFn(void*) {
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Power save: sau cửa sổ thức 30s, vào deep sleep theo sleepSeconds,
    // tỉnh bằng timer + chân touch/PIR. Logic này trước nằm trong loop()
    // nhưng task riêng là nơi đúng (loop vừa bận web/serial).
    unsigned long now = millis();
#ifndef SMARTFARM_TEST_HOOKS
    if (sys.powerSave && !demoRunning() && !sDebugMode && now >= 30000UL) {
      esp_sleep_enable_timer_wakeup((uint64_t)sys.sleepSeconds * 1000000ULL);
      esp_sleep_enable_ext1_wakeup((1ULL << TOUCH_PIN) | (1ULL << PIR_PIN), ESP_EXT1_WAKEUP_ANY_HIGH);
      LOG_INFO("Power save: entering deep sleep");
      Serial.flush();
      esp_deep_sleep_start();
    }
#endif
  }
}


// Test-only helper: exercise hầu hết module phụ trợ để tăng coverage.
// Chỉ gọi trong bản build SMARTFARM_TEST_HOOKS (xem runAutomation).
// KHÔNG gọi trên ESP32 production vì calibrationSave() ghi NVS mỗi lần.
#ifdef SMARTFARM_TEST_HOOKS
void runExtras() {
  rtosLock();
  rtosUnlock();
  (void)rtosLockTimeout(1);
  anomalyUpdate();
  (void)anomalyScore("temp");
  (void)anomalyIsHealthy("temp");
  (void)anomalyZScore(sys.gTemp, sys.tempThresholdHigh, 1.0f);
  (void)calibrationApplyTemp(sys.gTemp);
  (void)calibrationApplyHum(sys.gHum);
  (void)calibrationApplyLight(sys.gLight);
  (void)calibrationApplySoil(sys.gSoil);
  (void)calibrationApplyWater(sys.gWater);
  calibrationSetOffset("temp", 0.0f);
  calibrationSave();
  calibrationLoad();
  (void)pidUpdate(sys.tempThresholdHigh, sys.gTemp);
  pidReset();
  softStartUpdate();
  (void)softStartIsActive();
  (void)halAnalogRead(LDR_PIN);
  (void)halDigitalRead(TOUCH_PIN);
  halPwmWrite(FAN_PIN, 0);
  halI2cBegin(OLED_SDA, OLED_SCL);
  halSpiBegin();
  mqttLoop();
  (void)mqttConnected();
  (void)mqttPublish("t", "p");
  mqttSubscribe("t");
  websocketLoop();
  websocketBroadcast("m");
  tlsInit();
  (void)tlsEnabled();
  otaLoop();
  (void)otaVerifySignature(0, 0, 0, 0);
  metricsCollect();
  (void)metricsToJson();
  (void)metricsUptime();
  (void)healthCheck();
  (void)healthStatus();
  handleHealth();
}

#endif
