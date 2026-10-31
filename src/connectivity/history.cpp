#include "history.h"
#include "../globals.h"
#include "../logger.h"
#include "../control.h"
#include <math.h>
#include <stdarg.h>

#if HISTORY_ENABLED

static HistorySample sSamples[HISTORY_CAPACITY];
static int sHead = 0;          // vị trí ghi kế tiếp
static int sCount = 0;
static unsigned long sLastSampleMs = 0;
static bool sHasLastSample = false;

static void addSample(unsigned long now) {
  HistorySample& s = sSamples[sHead];
  s.seconds = (uint32_t)(now / 1000UL);
  s.temp = sys.gTemp;
  s.hum = sys.gHum;
  s.light = (uint16_t)constrain(sys.gLight, 0, 65535);
  s.soil = (uint16_t)constrain(sys.gSoil, 0, 65535);
  s.water = (uint16_t)constrain(sys.gWater, 0, 65535);
  s.flags = (sys.relayState ? 0x01 : 0) | (sys.fanState ? 0x02 : 0) |
            (manualAlarm() ? 0x04 : 0);

  sHead = (sHead + 1) % HISTORY_CAPACITY;
  if (sCount < HISTORY_CAPACITY) sCount++;
  sLastSampleMs = now;
  sHasLastSample = true;
}

void historyInit() {
  sHead = 0;
  sCount = 0;
  sLastSampleMs = 0;
  sHasLastSample = false;
}

void historyLoop(unsigned long now) {
  if (!sHasLastSample || now - sLastSampleMs >= HISTORY_INTERVAL_MS) {
    addSample(now);
  }
}

int historyCount() { return sCount; }

bool historyAt(int index, HistorySample* out) {
  if (!out || index < 0 || index >= sCount) return false;
  const int oldest = (sCount < HISTORY_CAPACITY) ? 0 : sHead;
  *out = sSamples[(oldest + index) % HISTORY_CAPACITY];
  return true;
}

// Định dạng dùng chung cho buildHistoryJson/historyJsonSize để Content-Length
// và thân response không bao giờ lệch nhau.
#define HISTORY_JSON_PREFIX_FMT "{\"interval\":%lu,\"capacity\":%d,\"count\":%d,\"samples\":["
static const char HISTORY_JSON_SUFFIX[] = "]}";
static const size_t HISTORY_SAMPLE_BUF_SIZE = 160;

// Kẹp một float về dạng in được TRƯỚC khi snprintf, để độ dài chuỗi luôn nằm
// trong định mức của scratch. %.1f của một float vô hạn/huge có thể dài tới ~40
// ký tự ("inf", hoặc 340282346638528859811000...0): khi đó scratch bị cắt, còn
// historyJsonSize() (tính theo số byte *lẽ ra* cần) vẫn cộng đủ -> Content-Length
// dài hơn thân, trình duyệt chờ dữ liệu tới timeout. Nguồn giá trị bất thường là
// flash hỏng hoặc phép toán tràn, không phải chỉ cảm biến.
static double printableTemp(float v) {
  if (!isfinite(v)) return 0.0;
  double d = v;
  if (d > 999.9) d = 999.9;
  else if (d < -999.9) d = -999.9;
  return d;
}

// Worst-case mỗi mẫu (dấu phẩy + 10 chữ số seconds + temp/hum 6 + ba số 16-bit
// + ba cờ + ngoặc) = 103 byte, nằm gọn trong HISTORY_SAMPLE_BUF_SIZE.
static int formatSample(const HistorySample& s, bool first, char* buf, size_t cap) {
  return snprintf(buf, cap,
                  "%s{\"t\":%lu,\"temp\":%.1f,\"hum\":%.1f,"
                  "\"light\":%u,\"soil\":%u,\"water\":%u,\"r\":%u,\"f\":%u,\"a\":%u}",
                  first ? "" : ",",
                  (unsigned long)s.seconds, printableTemp(s.temp), printableTemp(s.hum),
                  (unsigned)s.light, (unsigned)s.soil, (unsigned)s.water,
                  (unsigned)((s.flags & 0x01) ? 1 : 0),
                  (unsigned)((s.flags & 0x02) ? 1 : 0),
                  (unsigned)((s.flags & 0x04) ? 1 : 0));
}

// Bộ nối buffer có giới hạn: theo dõi (vị trí ghi, chỗ còn lại) tường minh
// thay vì tự tính lại độ dài sau mỗi snprintf. Cách cũ ("buf + (rem > 0 ? n : cap - 1)")
// chỉ cần lệch một byte là ghi đè 1 byte ngoài vùng đệm.
struct JsonWriter {
  char* buf;
  size_t cap;
  size_t pos;      // số byte đã ghi (không tính NUL)
  size_t need;     // tổng số byte LẼ RA cần, giống snprintf
  bool truncated;

  JsonWriter(char* b, size_t c) : buf(b), cap(c), pos(0), need(0), truncated(false) {
    if (buf && cap) buf[0] = '\0';
  }

  bool remaining() const { return buf != nullptr && pos + 1 < cap; }

  void append(const char* fmt, ...) {
    if (!remaining()) {
      truncated = true;
      need += 1;   // chỉ cần biết "đã tràn", giá trị chính xác không dùng
      return;
    }
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + pos, cap - pos, fmt, ap);
    va_end(ap);
    if (n < 0) {
      truncated = true;
      return;
    }
    need += (size_t)n;
    if ((size_t)n >= cap - pos) {
      truncated = true;
      pos = cap - 1;   // vsnprintf đã ghi phần vừa đủ + NUL
    } else {
      pos += (size_t)n;
    }
  }
};

int buildHistoryJson(char* buf, size_t cap) {
  if (!buf || cap == 0) return 0;

  JsonWriter w(buf, cap);
  w.append(HISTORY_JSON_PREFIX_FMT,
           (unsigned long)(HISTORY_INTERVAL_MS / 1000UL),
           (int)HISTORY_CAPACITY, sCount);

  char scratch[HISTORY_SAMPLE_BUF_SIZE];
  for (int i = 0; i < sCount; i++) {
    HistorySample s;
    historyAt(i, &s);   // i luôn nằm trong [0, sCount) nên luôn thành công
    formatSample(s, i == 0, scratch, sizeof(scratch));
    w.append("%s", scratch);
    if (w.truncated) break;
  }

  w.append(HISTORY_JSON_SUFFIX);
  // Hợp đồng giống snprintf: need >= cap nghĩa là JSON bị cắt cụt. Khi dừng
  // sớm vì tràn, ép need lên ít nhất cap để caller luôn phát hiện được.
  if (w.truncated && w.need < cap) w.need = cap;
  return (int)w.need;
}

size_t historyJsonSize() {
  char scratch[HISTORY_SAMPLE_BUF_SIZE];
  size_t total = (size_t)snprintf(scratch, sizeof(scratch), HISTORY_JSON_PREFIX_FMT,
                                  (unsigned long)(HISTORY_INTERVAL_MS / 1000UL),
                                  (int)HISTORY_CAPACITY, sCount);
  for (int i = 0; i < sCount; i++) {
    HistorySample s;
    historyAt(i, &s);   // i luôn nằm trong [0, sCount)
    total += (size_t)formatSample(s, i == 0, scratch, sizeof(scratch));
  }
  return total + sizeof(HISTORY_JSON_SUFFIX) - 1;
}

#ifdef SMARTFARM_TEST_HOOKS
void testResetHistoryStatics() { historyInit(); }
#endif

#else  // HISTORY_ENABLED

void historyInit() {}
void historyLoop(unsigned long) {}
int historyCount() { return 0; }
bool historyAt(int, HistorySample*) { return false; }
// Tắt lịch sử vẫn phải trả JSON đúng một body nhỏ: handler cấp phát theo
// historyJsonSize() rồi ghi Content-Length bằng đúng số byte đã dựng.
static const char HISTORY_OFF_JSON[] = "{\"count\":0,\"samples\":[]}";
int buildHistoryJson(char* buf, size_t cap) {
  if (!buf || cap == 0) return (int)sizeof(HISTORY_OFF_JSON) - 1;
  return snprintf(buf, cap, "%s", HISTORY_OFF_JSON);
}
size_t historyJsonSize() { return sizeof(HISTORY_OFF_JSON) - 1; }
#ifdef SMARTFARM_TEST_HOOKS
void testResetHistoryStatics() {}
#endif

#endif
