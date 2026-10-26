#pragma once
extern "C" {
#include "FreeRTOS.h"
#include <cstddef>

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *pcName,
                       uint32_t usStackDepth, void *pvParameters,
                       UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask);
void vTaskDelay(TickType_t xTicksToDelay);
TickType_t xTaskGetTickCount(void);
void vTaskDelete(TaskHandle_t xTask);
}
