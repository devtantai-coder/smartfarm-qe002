#include "../pid.h"
#include "../globals.h"

void pidInit(float kp, float ki, float kd) {
  sys.pidKp = kp; sys.pidKi = ki; sys.pidKd = kd;
  sys.pidIntegral = 0; sys.pidPrevError = 0; sys.pidLastOut = 0; sys.pidLastTime = millis();
}

float pidUpdate(float setpoint, float measured) {
  // Quy ước của dự án: đầu ra dương khi measured vượt setpoint
  // (dùng cho quạt: nhiệt độ càng cao thì duty càng lớn).
  float err = measured - setpoint;
  unsigned long now = millis();
  const unsigned long elapsedMs = now - sys.pidLastTime;

  // controlFan() được gọi MỖI vòng loop (~1 kHz) nhưng nhiệt độ chỉ có mẫu mới
  // mỗi SENSOR_INTERVAL. Tích phân theo từng vòng làm Ki/Kd lệch hàng nghìn lần
  // so với ý nghĩa "mỗi giây" của chúng (dt bị ép về 0,001 s), và lần gọi đầu
  // sau một khoảng nghỉ lại bơm một bước tích phân khổng lồ vào bộ nhớ PID.
  // Vì vậy chỉ tính lại khi đã có mẫu cảm biến mới; giữa hai mẫu trả lại đầu ra
  // cũ y nguyên.
  if (elapsedMs < SENSOR_INTERVAL) return sys.pidLastOut;

  // Một khoảng nghỉ dài (debug, demo, chờ WiFi) không được nhét cả khoảng nghỉ
  // vào một bước tích phân/phân hàm: mẫu cảm biến mới chỉ đại diện tối đa một
  // chu kỳ SENSOR_INTERVAL, nên dt bị kẹp về đúng một chu kỳ.
  float dt = (float)elapsedMs / 1000.0f;
  const float dtMax = (float)SENSOR_INTERVAL / 1000.0f;
  if (dt > dtMax) dt = dtMax;
  sys.pidIntegral += err * dt;
  if (sys.pidIntegral > 100.0f) sys.pidIntegral = 100.0f;
  if (sys.pidIntegral < -100.0f) sys.pidIntegral = -100.0f;
  float deriv = (err - sys.pidPrevError) / dt;
  float out = sys.pidKp * err + sys.pidKi * sys.pidIntegral + sys.pidKd * deriv;
  if (out > 255.0f) out = 255.0f;
  if (out < 0.0f) out = 0.0f;
  sys.pidPrevError = err;
  sys.pidLastOut = out;
  sys.pidLastTime = now;
  return out;
}

void pidReset() {
  sys.pidIntegral = 0;
  sys.pidPrevError = 0;
  sys.pidLastOut = 0;
  sys.pidLastTime = millis();
}
