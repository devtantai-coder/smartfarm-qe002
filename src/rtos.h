#ifndef SMARTFARM_RTOS_H
#define SMARTFARM_RTOS_H

#include "config.h"

#if defined(SMARTFARM_TEST_HOOKS)
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#else
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>
#endif

// ── Cấu hình task ──
#define RTOS_SENSOR_STACK   2048
#define RTOS_CONTROL_STACK  4096
#define RTOS_COMM_STACK     8192
#define RTOS_DISPLAY_STACK  2048
#define RTOS_POWER_STACK    1024

#define RTOS_SENSOR_PRIO    2
#define RTOS_CONTROL_PRIO   3
#define RTOS_COMM_PRIO      4
#define RTOS_DISPLAY_PRIO   1
#define RTOS_POWER_PRIO     0

// Event bits
#define EVT_SENSOR_READY    (1 << 0)
#define EVT_COMMAND         (1 << 1)
#define EVT_OLED_DIRTY      (1 << 2)
#define EVT_POWER_SAVE      (1 << 3)
#define EVT_OTA_START       (1 << 4)
#define EVT_DEMO_RUNNING    (1 << 5)

// Handles chia sẻ
extern SemaphoreHandle_t sysMutex;
extern EventGroupHandle_t evtGroup;
extern QueueHandle_t qSensorData;
extern QueueHandle_t qCommand;
extern QueueHandle_t qDisplayUpdate;

// Struct dữ liệu trao đổi
struct SensorData {
  float temp, hum;
  int light, soil, water;
  bool pir, touch;
};
struct Command {
  int id;
  int value;
};
struct DisplaySnapshot {
  float temp, hum;
  int light, soil, water;
};

void rtosInit();
void rtosLock();
void rtosUnlock();
bool rtosLockTimeout(uint32_t ms);

#endif
