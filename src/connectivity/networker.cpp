#include "networker.h"
#include "../config.h"
#include "../logger.h"
#include "notify.h"
#include "weather.h"

#include <stdint.h>
#include <string.h>

#if defined(SMARTFARM_TEST_HOOKS)

// ── Bản build test trên host ──
// Không có FreeRTOS thật (xTaskCreate của mock không chạy task), và mọi nhánh
// mạng đã bị biên dịch bỏ, nên worker chỉ cần tồn tại để link. Bộ test kiểm
// tra logic thông báo/thời tiết qua hook đồng bộ của từng module.
void netWorkerInit() {}
bool netWorkerPostNotify(const char*, const char*) { return false; }
bool netWorkerPostWeather() { return false; }
void testResetNetWorker() {}

#else

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "../irqlock.h"

namespace {

enum NetJobKind : uint8_t {
  NETJOB_NOTIFY = 1,
  NETJOB_WEATHER = 2,
};

struct NetJob {
  uint8_t kind;
  char tag[16];      // loại sự kiện, dùng cho cửa sổ chống spam bên notify.cpp
  char message[128];
};

QueueHandle_t sQueue = nullptr;
TaskHandle_t sTask = nullptr;
// netWorkerInit() được gọi lại từ hai hàm post nếu lần tạo queue/task đầu tiên
// thất bại (thiếu RAM lúc boot). Không có giới hạn thì mỗi sự kiện lại thử tạo
// task; quá dày thì log ngập. 10 giây giữa hai lần thử là đủ.
unsigned long sLastInitAttempt = 0;
static constexpr unsigned long NET_INIT_RETRY_MS = 10000UL;
// Gộp nhiều yêu cầu thời tiết liên tiếp thành một job đang chờ trong queue.
// loopTask đặt cờ, tác vụ nền xóa cờ — hai nhân cùng sửa một biến thì phải có
// khoá, nếu không một lần đặt có thể bị mất và /weather vĩnh viễn không được
// cập nhật nữa (weatherLoop luôn thấy "đã có job chờ").
bool sWeatherQueued = false;
IrqLock sWeatherQueuedLock = IRQLOCK_INIT;

void netTaskBody(void*) {
  NetJob job;
  for (;;) {
    if (xQueueReceive(sQueue, &job, portMAX_DELAY) != pdTRUE) continue;

    if (job.kind == NETJOB_NOTIFY) {
      const bool sent = notifySendNow(job.tag, job.message);
      // Chỉ "tiêu" cửa sổ chống spam khi gửi thành công; gửi hỏng thì mở lại
      // cửa sổ sau TELEGRAM_RETRY_MS để lần sau còn cơ hội tới tay người dùng.
      notifyComplete(job.tag, sent);
    } else {
      {
        IrqGuard g(&sWeatherQueuedLock);
        sWeatherQueued = false;
      }
      weatherFetchNow();
    }

    // Nhường CPU cho loopTask/web server giữa hai job.
    vTaskDelay(1);
  }
}

}  // namespace

void netWorkerInit() {
  if (sQueue != nullptr) return;
  const unsigned long now = millis();
  if (sLastInitAttempt != 0 && now - sLastInitAttempt < NET_INIT_RETRY_MS) return;
  sLastInitAttempt = now;
  sQueue = xQueueCreate(NETWORK_QUEUE_LEN, sizeof(NetJob));
  if (sQueue == nullptr) {
    LOG_ERROR_CODE(602, "Network: cannot create queue");
    return;
  }
  {
    IrqGuard g(&sWeatherQueuedLock);
    sWeatherQueued = false;
  }
  if (xTaskCreate(netTaskBody, "networker", NETWORK_TASK_STACK, nullptr,
                  NETWORK_TASK_PRIORITY, &sTask) != pdPASS) {
    vQueueDelete(sQueue);
    sQueue = nullptr;
    LOG_ERROR_CODE(603, "Network: cannot create task");
    return;
  }
  LOG_INFO("Network worker started");
}

bool netWorkerPostNotify(const char* kind, const char* message) {
  if (kind == nullptr || message == nullptr) return false;
  if (sQueue == nullptr) netWorkerInit();   // task chết lúc boot: thử lại, có giãn cách
  if (sQueue == nullptr) return false;
  NetJob job = {};
  job.kind = NETJOB_NOTIFY;
  strncpy(job.tag, kind, sizeof(job.tag) - 1);
  strncpy(job.message, message, sizeof(job.message) - 1);
  // 0 tick: không bao giờ chặn loop(), kể cả khi hàng đợi đầy.
  if (xQueueSend(sQueue, &job, 0) != pdTRUE) {
    LOG_WARN("Network: notify queue full, dropped");
    return false;
  }
  return true;
}

bool netWorkerPostWeather() {
  if (sQueue == nullptr) netWorkerInit();   // task chết lúc boot: thử lại, có giãn cách
  if (sQueue == nullptr) return false;
  IrqGuard g(&sWeatherQueuedLock);
  if (sWeatherQueued) return true;      // đã có job chờ: gộp, không xếp thêm
  NetJob job = {};
  job.kind = NETJOB_WEATHER;
  if (xQueueSend(sQueue, &job, 0) != pdTRUE) return false;
  sWeatherQueued = true;
  return true;
}

#endif
