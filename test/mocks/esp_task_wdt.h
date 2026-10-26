#pragma once
#include <cstdint>

struct esp_task_wdt_config_t {
  uint32_t timeout_ms;
  uint32_t idle_core_mask;
  bool trigger_panic;
};

int esp_task_wdt_init(const esp_task_wdt_config_t *config);
int esp_task_wdt_init(uint32_t timeout_s, bool panic);
int esp_task_wdt_add(void *handle);
int esp_task_wdt_reset();
int esp_task_wdt_deinit();
