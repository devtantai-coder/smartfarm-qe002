#ifndef SMARTFARM_IRQLOCK_H
#define SMARTFARM_IRQLOCK_H

// Khoá ngắn dùng giữa loopTask và tác vụ mạng nền (networker).
//
// Hai task chạy trên hai nhân thật, vì vậy mọi biến chung đều phải được chép
// DƯỚI khoá — kể cả "chỉ đọc một struct nhỏ". Không có atomic nào bảo vệ bản
// thân struct: loopTask có thể thấy nửa dữ liệu mới nửa cũ, và một chuỗi đang
// được ghi dở thì "%s" in ra có nguy cơ đọc tràn ra ngoài vùng đang chép.
//
// Chỉ giữ khoá trong lúc chép dữ liệu, không giữ lúc gọi snprintf/HTTP:
// portENTER_CRITICAL tắt ngắt trên nhân đang chạy.
//
// Bản test trên host dùng std::mutex thay vì spinlock: đơn luồng thì nó vô hại,
// còn khi chạy dưới -fsanitize=thread nó tố ngay nếu một bên quên khoá.
#if defined(SMARTFARM_TEST_HOOKS)

#include <mutex>

struct IrqLock {
  std::mutex m;
};
#define IRQLOCK_INIT {}
static inline void irqLockEnter(IrqLock* l) { l->m.lock(); }
static inline void irqLockExit(IrqLock* l) { l->m.unlock(); }

#else

#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

typedef portMUX_TYPE IrqLock;
#define IRQLOCK_INIT portMUX_INITIALIZER_UNLOCKED
static inline void irqLockEnter(IrqLock* l) { portENTER_CRITICAL(l); }
static inline void irqLockExit(IrqLock* l) { portEXIT_CRITICAL(l); }

#endif

// Guard theo kiểu RAII — không có đường nào thoát khỏi hàm mà quên nhả khoá.
struct IrqGuard {
  IrqLock* lock;
  explicit IrqGuard(IrqLock* l) : lock(l) { irqLockEnter(l); }
  ~IrqGuard() { irqLockExit(lock); }
  IrqGuard(const IrqGuard&) = delete;
  IrqGuard& operator=(const IrqGuard&) = delete;
};

#endif
