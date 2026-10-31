#include "notify.h"
#include "../config.h"
#include "../globals.h"
#include "../logger.h"
#include "../wifi.h"
#include "../irqlock.h"
#include "networker.h"

#if !defined(SMARTFARM_TEST_HOOKS)
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

static bool sEnabled = false;
static constexpr int RATE_SLOTS = 4;
static const char* sRateKind[RATE_SLOTS] = {nullptr};
static unsigned long sRateLast[RATE_SLOTS] = {0};
// reserveSlot() chạy trên loopTask, notifyComplete() chạy trên tác vụ mạng nền
// -> hai bên cùng sửa mảng slot. Không khoá là cửa sổ chống spam quyết định
// theo dữ liệu đang bị ghi dở (TSan báo race trên bản build có thread thật).
static IrqLock sRateLock = IRQLOCK_INIT;

// Cửa sổ chống spam: mỗi LOẠI sự kiện chỉ gửi tối đa một lần trong
// TELEGRAM_MIN_INTERVAL_MS. Slot được GIỮ CHỖ ngay khi xếp job (để hai sự kiện
// dồn dập không cùng lọt qua trong lúc job đầu còn chờ gửi), và được nhả sớm
// nếu lần gửi đó thất bại — trước đây gửi hỏng vẫn mất trọn 30 giây lặng.
static bool reserveSlot(const char* kind, unsigned long now) {
  IrqGuard g(&sRateLock);
  int freeSlot = -1;
  int oldestSlot = 0;
  for (int i = 0; i < RATE_SLOTS; i++) {
    if (sRateKind[i] && strcmp(sRateKind[i], kind) == 0) {
      if (now - sRateLast[i] < TELEGRAM_MIN_INTERVAL_MS) return false;
      sRateLast[i] = now;
      return true;
    }
    if (!sRateKind[i] && freeSlot < 0) freeSlot = i;
    if (sRateLast[i] < sRateLast[oldestSlot]) oldestSlot = i;
  }
  // Hết slot: tái dùng slot cũ nhất. Trước đây nhánh này trả true mà KHÔNG ghi
  // slot, nên một loại sự kiện mới không hề bị giới hạn tần suất (mỗi lần gọi
  // đều xếp job) và cũng không bao giờ có slot riêng để nhả khi gửi hỏng.
  const int slot = (freeSlot >= 0) ? freeSlot : oldestSlot;
  sRateKind[slot] = kind;
  sRateLast[slot] = now;
  return true;
}

// Gửi hỏng: lùi mốc về quá khứ sao cho lần thử kế tiếp được phép sau
// TELEGRAM_RETRY_MS (thay vì chờ hết TELEGRAM_MIN_INTERVAL_MS).
void notifyComplete(const char* kind, bool sent) {
  if (sent || !kind) return;
  const unsigned long now = millis();
  const unsigned long backoff = TELEGRAM_MIN_INTERVAL_MS > TELEGRAM_RETRY_MS
                                    ? TELEGRAM_MIN_INTERVAL_MS - TELEGRAM_RETRY_MS
                                    : 0UL;
  IrqGuard g(&sRateLock);
  for (int i = 0; i < RATE_SLOTS; i++) {
    if (sRateKind[i] && strcmp(sRateKind[i], kind) == 0) {
      sRateLast[i] = now - backoff;
      return;
    }
  }
}

void notifyInit() {
  sEnabled = (TELEGRAM_BOT_TOKEN[0] != '\0' && TELEGRAM_CHAT_ID[0] != '\0');
  {
    // Khóa chỉ bọc phần ghi mảng slot: LOG_* ở ngoài vì portENTER_CRITICAL
    // tắt ngắt trên nhân đang chạy, còn Serial.printf kéo dài hàng trăm
    // micro-giây.
    IrqGuard g(&sRateLock);
    for (int i = 0; i < RATE_SLOTS; i++) {
      sRateKind[i] = nullptr;
      sRateLast[i] = 0;
    }
  }
  if (sEnabled) LOG_INFO("Telegram notifications enabled");
  else LOG_DEBUG("Telegram notifications disabled");

  // Mặc định vẫn là INSECURE=1 để kit chạy được ngay, nhưng người dùng phải thấy
  // đánh đổi: không pin CA thì bot token + nội dung cảnh báo đi qua một đường TLS
  // mà bất kỳ ai đứng giữa cũng nghe/decrypted được. Báo MỘT lần mỗi lần khởi
  // động (notifyInit chỉ chạy một lần), không báo lại mỗi lần gửi.
  if (sEnabled && TELEGRAM_TLS_INSECURE && TELEGRAM_CA_CERT[0] == '\0') {
    LOG_ERROR("Telegram: TLS NOT verified (TELEGRAM_TLS_INSECURE=1, no CA pinned) - bot token can be intercepted. Set TELEGRAM_CA_CERT in src/secrets.h");
  }
}

bool notifyEnabled() { return sEnabled; }

#if defined(SMARTFARM_TEST_HOOKS)
static char sLastEvent[32] = "";
static char sLastMessage[128] = "";
#endif

#if !defined(SMARTFARM_TEST_HOOKS)
static size_t urlEncode(char* out, size_t cap, const char* in) {
  static const char* hex = "0123456789ABCDEF";
  size_t n = 0;
  for (const unsigned char* p = (const unsigned char*)in; *p; p++) {
    const bool safe = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                      (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' ||
                      *p == '.' || *p == '~';
    if (safe) {
      if (n + 1 >= cap) break;
      out[n++] = (char)*p;
    } else {
      if (n + 3 >= cap) break;
      out[n++] = '%';
      out[n++] = hex[*p >> 4];
      out[n++] = hex[*p & 0x0F];
    }
  }
  out[n] = '\0';
  return n;
}

// Đọc dòng trạng thái "HTTP/1.1 200 OK" và trả mã số. Dùng vTaskDelay(1)
// giữa các lần đọc thay vì busy-wait: task nền không được chiếm CPU của loopTask.
static int readHttpStatus(WiFiClientSecure& client, unsigned long deadline) {
  char line[48];
  size_t n = 0;
  while ((long)(deadline - millis()) > 0) {
    if (client.available() <= 0) {
      if (!client.connected()) break;
      vTaskDelay(1);
      continue;
    }
    const int c = client.read();
    if (c < 0) continue;
    if (c == '\n') break;
    if (c != '\r' && n + 1 < sizeof(line)) line[n++] = (char)c;
  }
  line[n] = '\0';
  if (strncmp(line, "HTTP/", 5) != 0) return 0;
  const char* sp = strchr(line, ' ');
  return sp ? atoi(sp + 1) : 0;
}
#endif  // !SMARTFARM_TEST_HOOKS

// Gửi thật một thông báo. Chỉ được gọi từ tác vụ mạng nền (có thể chặn vài
// giây vì bắt tay TLS) — không bao giờ gọi trực tiếp trong loop().
bool notifySendNow(const char* kind, const char* message) {
#if defined(SMARTFARM_TEST_HOOKS)
  strncpy(sLastEvent, kind, sizeof(sLastEvent) - 1);
  sLastEvent[sizeof(sLastEvent) - 1] = '\0';
  strncpy(sLastMessage, message, sizeof(sLastMessage) - 1);
  sLastMessage[sizeof(sLastMessage) - 1] = '\0';
  return true;
#else
  // Tác vụ nền: hỏi driver thẳng, không chạm cache WiFi của loopTask.
  if (!wifiConnectedFresh()) return false;

  char text[160];
  urlEncode(text, sizeof(text), message);
  char body[288];
  int bodyLen = snprintf(body, sizeof(body), "chat_id=%s&text=%s",
                         TELEGRAM_CHAT_ID, text);
  if (bodyLen <= 0 || (size_t)bodyLen >= sizeof(body)) return false;

  WiFiClientSecure client;
  if (strlen(TELEGRAM_CA_CERT) > 0) {
    client.setCACert(TELEGRAM_CA_CERT);
  } else if (TELEGRAM_TLS_INSECURE) {
    client.setInsecure();   // không xác thực chứng chỉ: chỉ nên dùng khi thử nghiệm
  } else {
    LOG_WARN("Telegram: no CA pinned, refusing insecure TLS");
    return false;
  }

  // connect() chỉ giới hạn phần TCP; bắt tay TLS dùng handshake_timeout riêng
  // (mặc định của core có thể tới 120 s — tác vụ mạng nền sẽ treo cả phút).
  client.setHandshakeTimeout((TELEGRAM_TIMEOUT_MS + 999UL) / 1000UL);

  if (!client.connect("api.telegram.org", 443, (int)TELEGRAM_TIMEOUT_MS)) {
    LOG_WARN("Telegram: cannot connect");
    return false;
  }

  // Đặt mốc sau khi kết nối xong: bắt tay TLS có thể ăn hết timeout kết nối,
  // nếu tính trước thì phần đọc status còn 0 ms và bị báo hỏng oan.
  const unsigned long deadline = millis() + TELEGRAM_TIMEOUT_MS;

  char header[320];
  int headerLen = snprintf(header, sizeof(header),
                           "POST /bot%s/sendMessage HTTP/1.1\r\n"
                           "Host: api.telegram.org\r\n"
                           "Content-Type: application/x-www-form-urlencoded\r\n"
                           "Content-Length: %d\r\n"
                           "Connection: close\r\n\r\n",
                           TELEGRAM_BOT_TOKEN, bodyLen);
  if (headerLen <= 0 || (size_t)headerLen >= sizeof(header)) {
    client.stop();
    return false;
  }
  client.print(header);
  client.print(body);

  const int status = readHttpStatus(client, deadline);
  client.stop();
  // Chỉ coi là đã gửi khi Telegram trả 2xx: trước đây log "alert sent" kể cả
  // khi kết nối rỗng hoặc token sai.
  if (status < 200 || status >= 300) {
    LOG_WARN("Telegram: send failed");
    return false;
  }
  LOG_INFO("Telegram: alert sent");
  return true;
#endif
}

bool notifyEvent(const char* kind, const char* message) {
  if (!sEnabled || !kind || !message) return false;
  if (!reserveSlot(kind, millis())) return false;

  // Từ đây chỉ xếp job — loop() không bao giờ chờ mạng.
#if defined(SMARTFARM_TEST_HOOKS)
  return notifySendNow(kind, message);
#else
  if (!wifiConnected() || !netWorkerPostNotify(kind, message)) {
    notifyComplete(kind, false);   // nhả slot để lần sau còn thử lại
    return false;
  }
  return true;
#endif
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetNotifyStatics() {
  sEnabled = (TELEGRAM_BOT_TOKEN[0] != '\0' && TELEGRAM_CHAT_ID[0] != '\0');
  for (int i = 0; i < RATE_SLOTS; i++) {
    sRateKind[i] = nullptr;
    sRateLast[i] = 0;
  }
  sLastEvent[0] = '\0';
  sLastMessage[0] = '\0';
}
void testNotifyForceEnabled(bool on) { sEnabled = on; }
const char* testNotifyLastEvent() { return sLastEvent; }
const char* testNotifyLastMessage() { return sLastMessage; }
#endif
