#include "../rtos.h"
#include "../globals.h"

SemaphoreHandle_t sysMutex = NULL;
EventGroupHandle_t evtGroup = NULL;
QueueHandle_t qSensorData = NULL;
QueueHandle_t qCommand = NULL;
QueueHandle_t qDisplayUpdate = NULL;

void rtosInit() {
  if (sysMutex == NULL) sysMutex = xSemaphoreCreateRecursiveMutex();
  if (evtGroup == NULL) evtGroup = xEventGroupCreate();
  if (qSensorData == NULL) qSensorData = xQueueCreate(4, sizeof(SensorData));
  if (qCommand == NULL) qCommand = xQueueCreate(8, sizeof(Command));
  if (qDisplayUpdate == NULL) qDisplayUpdate = xQueueCreate(2, sizeof(DisplaySnapshot));
}

void rtosLock() {
  if (sysMutex) xSemaphoreTakeRecursive(sysMutex, portMAX_DELAY);
}

void rtosUnlock() {
  if (sysMutex) xSemaphoreGiveRecursive(sysMutex);
}

bool rtosLockTimeout(uint32_t ms) {
  if (!sysMutex) return true;
  return xSemaphoreTakeRecursive(sysMutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
