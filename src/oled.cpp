#include "globals.h"
#include "logger.h"
#include "sensors.h"
#include "control.h"
#include "demo.h"
#include "oled.h"

// Nhãn chỉ dùng ASCII: font mặc định SSD1306 không có dấu tiếng Việt
static const char* weatherText() {
  switch (weatherStatus()) {
    case WEATHER_SUNNY:  return "Nang";
    case WEATHER_CLOUDY: return "Am u";
    case WEATHER_RAIN:   return "Mua";
    default:             return "B.thuong";
  }
}

// ── Đẩy framebuffer qua I2C theo vùng thực sự đổi ──
// display.display() của thư viện luôn gửi đủ 1024 byte (~23 ms ở 400 kHz).
// Ở đây chỉ gửi đúng vùng pixel thay đổi: theo từng page 8-hàng, và trong
// mỗi page chỉ gửi dải cột đã đổi (mỗi ký tự chiếm 6 cột với font mặc định).
// Ví dụ đổi "H2O:900" -> "H2O:500" chỉ đổi 1 ký tự = 6 cột = 6 byte thay vì
// gửi lại cả page 128 byte — giảm ~95% lưu lượng I2C và rút ngắn thời gian
// chặn loop() (mỗi byte I2C ở 400 kHz tốn ~22 µs).
static uint8_t sPrevFrame[SCREEN_WIDTH * SCREEN_HEIGHT / 8];
static bool sPrevFrameValid = false;

// Màn hình có dùng được không? false khi không tìm thấy lúc khởi động, hoặc
// khi I2C hỏng giữa chừng (tự tắt để loop không bị nghẽn bởi giao dịch lỗi).
static bool sOledOk = false;

// Số lần lỗi I2C liên tiếp. Mỗi lần lỗi tốn tới OLED_I2C_TIMEOUT_MS nên
// sau vài lần liên tiếp sẽ tự tắt màn hình thay vì cứ thử mãi.
static uint8_t sOledFailStreak = 0;
static const uint8_t OLED_FAIL_LIMIT = 3;

// 127 byte dữ liệu + 1 byte điều khiển = 128 = buffer I2C mặc định của Wire.
static const size_t OLED_CHUNK = 127;

static bool oledWriteData(const uint8_t* data, size_t n) {
  bool ok = true;
  size_t sent = 0;
  while (sent < n) {
    size_t chunk = n - sent;
    if (chunk > OLED_CHUNK) chunk = OLED_CHUNK;
    Wire.beginTransmission(OLED_I2C_ADDR);
    Wire.write((uint8_t)0x40);   // control: D/C# = 1 (luồng dữ liệu)
    Wire.write(data + sent, chunk);
    if (Wire.endTransmission() != 0) ok = false;
    sent += chunk;
  }
  return ok;
}

// Ghi một dải cột [c0..c1] của một page.
//
// SSD1306 đang ở chế độ địa chỉ ngang (horizontal addressing, 0x20 0x00 —
// thư viện Adafruit_SSD1306 đặt trong begin()): lệnh 0x21 đặt cột đầu/cuối,
// lệnh 0x22 đặt page đầu/cuối. Con trỏ cột tự tăng khi ghi byte và quay về
// cột đầu khi chạm cột cuối — nhưng vì ta ghi ĐÚNG (c1-c0+1) byte và đặt page
// là một page duy nhất (p..p) nên con trỏ không bao giờ vượt qua cột cuối và
// không nhảy sang page kế tiếp. Vì vậy ghi đúng số byte là đủ.
static bool oledWriteCols(const uint8_t* pageData, uint8_t page, uint8_t c0, uint8_t c1) {
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write((uint8_t)0x00);                 // control: toàn lệnh
  Wire.write((uint8_t)0x21);                 // set column address
  Wire.write(c0);                            // từ cột c0
  Wire.write(c1);                            // đến cột c1
  Wire.write((uint8_t)0x22);                 // set page address
  Wire.write(page);
  Wire.write(page);                          // chỉ một page
  bool ok = (Wire.endTransmission() == 0);
  return oledWriteData(pageData + c0, (size_t)(c1 - c0 + 1)) && ok;
}

// Ghi trọn dải page liên tiếp [p0..p1] bằng MỘT lệnh địa chỉ + một luồng dữ
// liệu. Dùng cho lần vẽ đầu tiên (màn hình trắng) — ít giao dịch I2C nhất.
static bool oledWritePages(const uint8_t* frame, uint8_t p0, uint8_t p1) {
  Wire.beginTransmission(OLED_I2C_ADDR);
  Wire.write((uint8_t)0x00);                 // control: toàn lệnh
  Wire.write((uint8_t)0x21);                 // set column address
  Wire.write((uint8_t)0);                    // từ cột 0
  Wire.write((uint8_t)(SCREEN_WIDTH - 1));   // đến cột cuối
  Wire.write((uint8_t)0x22);                 // set page address
  Wire.write(p0);
  Wire.write(p1);
  bool ok = (Wire.endTransmission() == 0);
  return oledWriteData(frame + (size_t)p0 * SCREEN_WIDTH, (size_t)(p1 - p0 + 1) * SCREEN_WIDTH) && ok;
}

bool oledInit() {
  // I2C 400 kHz (SSD1306 hỗ trợ tối đa 400 kHz) — display() đẩy cả framebuffer
  // 1024 byte nên tăng tốc I2C giúp giảm mạnh thời gian chặn loop.
  Wire.begin(OLED_SDA, OLED_SCL, 400000);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // Giới hạn thời gian chờ một giao dịch I2C (mặc định 50 ms quá dài khi
  // màn hình không cắm: mỗi giao dịch lỗi sẽ chặn loop tới 50 ms).
  Wire.setTimeOut(OLED_I2C_TIMEOUT_MS);
#endif

  // Dò màn hình TRƯỚC khi gọi display.begin(): begin() gửi hàng chục lệnh
  // khởi tạo, mỗi lệnh là một giao dịch I2C — nếu không có màn hình thì
  // mỗi giao dịch đều chờ hết thời gian chờ. Dò trước bằng MỘT giao dịch
  // giữ lại nguyên trạng thái "không có màn hình" chỉ tốn 1 lần chờ.
  Wire.beginTransmission(OLED_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    sOledOk = false;
    return false;
  }

  sOledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);
  if (!sOledOk) return false;
  sOledFailStreak = 0;
  display.clearDisplay();
  display.display();
  sPrevFrameValid = false;    // khung so sánh chưa hợp lệ -> lần vẽ sau ghi lại trọn
  return true;
}

bool oledAvailable() { return sOledOk; }

void oledForceRedraw() { sPrevFrameValid = false; }

// Ghi nhận một lần lỗi I2C:
// - Khung so sánh không còn đúng (màn hình có thể đã lệch so với buffer) ->
//   đánh dấu để lần vẽ sau ghi lại TOÀN BỘ, không chỉ phần đổi.
// - Sau vài lần lỗi liên tiếp thì tự tắt màn hình: mỗi giao dịch lỗi đều tốn
//   tới OLED_I2C_TIMEOUT_MS, cứ thử mãi chỉ làm nghẽn loop().
static void oledNoteFailure() {
  sPrevFrameValid = false;
  if (++sOledFailStreak >= OLED_FAIL_LIMIT) {
    sOledOk = false;
    LOG_ERROR_CODE(103, "OLED I2C failed - disabled");
  }
}

void updateOLED(float t, float h, int light, int soil, int water) {
  // Không có màn hình (không cắm, hoặc I2C đã hỏng) -> khỏi vẽ, khỏi đụng
  // I2C. Mỗi giao dịch I2C lỗi chặn loop tới hết thời gian chờ, vô ích mà
  // còn làm nghẽn cả web server.
  if (!sOledOk) return;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  // Thanh tiêu đề
  display.setCursor(0, 0);
  display.print(F("Smart Farm"));

  // Dòng 1: trạng thái thời tiết (hoạt động 11)
  display.setCursor(0, 8);
  display.print(F("TT: "));
  display.print(weatherText());

  // Dòng 2: Nhiệt độ + Độ ẩm
  display.setCursor(0, 16);
  display.print(F("T:"));
  display.print(t, 1);
  display.print(F("C H:"));
  display.print(h, 0);
  display.print(F("%"));

  // Dòng 3: Ánh sáng
  display.setCursor(0, 24);
  display.print(F("Light: "));
  display.print(light);

  // Dòng 4: Đất
  display.setCursor(0, 32);
  display.print(F("Soil: "));
  display.print(soil);

  // Dòng 5: Nước + trạng thái máy bơm
  display.setCursor(0, 40);
  display.print(F("H2O:"));
  display.print(water);
  display.print(F(" P:"));
  display.print(sys.relayState ? F("ON") : F("OFF"));

  // Dòng 6: IP WiFi — cache chuỗi IP để không cấp phát String mỗi lần vẽ.
  display.setCursor(0, 48);
  if (WiFi.status() == WL_CONNECTED) {
    static IPAddress sCachedIp;
    static char sIpStr[16] = {0};
    IPAddress ip = WiFi.localIP();
    if (sIpStr[0] == '\0' || ip != sCachedIp) {
      sCachedIp = ip;
      String ts = ip.toString();
      strncpy(sIpStr, ts.c_str(), sizeof(sIpStr) - 1);
      sIpStr[sizeof(sIpStr) - 1] = '\0';
    }
    display.print(F("IP:"));
    display.print((const char*)sIpStr);
  } else {
    display.print(F("WiFi: ---"));
  }

  // Dòng 7: trạng thái demo / cơ cấu
  display.setCursor(0, 56);
  if (demoRunning()) {
    display.print(demoStepName());
  } else if (sys.windmillOn) {
    display.print(F("Coi xay: ON"));
  } else if (feedRunning()) {
    display.print(F("Cho an..."));
  } else {
    display.print(F("San sang"));
  }

  const uint8_t pages = SCREEN_HEIGHT / 8;
  const uint8_t* frame = display.getBuffer();

  // Lần vẽ đầu tiên (màn hình vừa bật): ghi trọn màn hình bằng một lệnh
  // địa chỉ duy nhất + một luồng dữ liệu — ít giao dịch I2C nhất.
  if (!sPrevFrameValid) {
    memcpy(sPrevFrame, frame, sizeof(sPrevFrame));
    sPrevFrameValid = true;
    if (!oledWritePages(frame, 0, pages - 1)) oledNoteFailure();
    return;
  }

  // Các lần sau: quét từng page, tìm dải cột đầu..cuối bị đổi so với khung
  // trước và chỉ gửi đúng vùng đó.
  bool wroteAny = false;
  bool ok = true;
  for (uint8_t p = 0; p < pages; p++) {
    const uint8_t* cur  = frame + (size_t)p * SCREEN_WIDTH;
    const uint8_t* prev = sPrevFrame + (size_t)p * SCREEN_WIDTH;

    // So sánh nguyên page bằng memcmp trước: page không đổi thì bỏ qua ngay,
    // tránh quét 128 cột cho mỗi page không thay đổi.
    if (memcmp(cur, prev, SCREEN_WIDTH) == 0) continue;

    uint8_t c0 = 0xFF, c1 = 0;
    for (uint8_t c = 0; c < SCREEN_WIDTH; c++) {
      if (cur[c] != prev[c]) {
        if (c0 == 0xFF) c0 = c;
        c1 = c;
      }
    }
    if (c0 == 0xFF) continue;                // page này không đổi -> không đụng I2C

    if (!oledWriteCols(cur, p, c0, c1)) ok = false;
    wroteAny = true;
  }

  // Không có gì đổi: giữ nguyên khung trước, không ghi lại (không tốn I2C).
  if (!wroteAny) return;

  memcpy(sPrevFrame, frame, sizeof(sPrevFrame));

  // I2C hỏng giữa chừng (màn hình bị rút, bus lỗi): sau vài lần lỗi liên
  // tiếp thì tự tắt màn hình thay vì cứ thử mãi và liên tục chặn loop.
  if (!ok) {
    oledNoteFailure();
  } else {
    sOledFailStreak = 0;
  }
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: coi như màn hình vừa bật (khung trước không hợp lệ).
void testResetOledStatics() { sPrevFrameValid = false; }
#endif
