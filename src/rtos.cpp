#include "rtos.h"
#include "globals.h"

SemaphoreHandle_t sysMutex = NULL;
EventGroupHandle_t evtGroup = NULL;
QueueHandle_t qSensorData = NULL;
QueueHandle_t qCommand = NULL;
QueueHandle_t qDisplayUpdate = NULL;

void rtosInit() {
  if (sysMutex == NULL) sysMutex = xSemaphoreCreateMutex();
  if (evtGroup == NULL) evtGroup = xEventGroupCreate();
  if (qSensorData == NULL) qSensorData = xQueueCreate(4, sizeof(SensorData));
  if (qCommand == NULL) qCommand = xQueueCreate(8, sizeof(Command));
  if (qDisplayUpdate == NULL) qDisplayUpdate = xQueueCreate(2, sizeof(DisplaySnapshot));
}

void rtosLock() {
  if (sysMutex) xSemaphoreTake(sysMutex, portMAX_DELAY);
}

void rtosUnlock() {
  if (sysMutex) xSemaphoreGive(sysMutex);
}

bool rtosLockTimeout(uint32_t ms) {
  if (!sysMutex) return true;
  return xSemaphoreTake(sysMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
