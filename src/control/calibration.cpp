#include "../calibration.h"
#include "../globals.h"
#include "../logger.h"
#include <Preferences.h>

static Preferences sPrefs;

void calibrationInit() { calibrationLoad(); }

// Kẹp kết quả sau hiệu chuẩn về miền ADC [0,4095]. sensors.cpp cũng kẹp một
// lần nữa (hai tầng độc lập), nhưng hàm apply phải tự đúng: nơi khác gọi trực
// tiếp sẽ nhận giá trị âm/>4095 và logic so ngưỡng sẽ sai.
static float clampAdcValue(float v) {
  if (!(v > 0.0f)) return 0.0f;   // NaN/Inf rơi vào đây
  if (v > 4095.0f) return 4095.0f;
  return v;
}

float calibrationApplyTemp(float raw) { return (raw + sys.calTempOffset) * sys.calTempGain; }
float calibrationApplyHum(float raw) { return (raw + sys.calHumOffset) * sys.calHumGain; }
float calibrationApplyLight(int raw) { return clampAdcValue((float)raw + sys.calLightOffset); }
float calibrationApplySoil(int raw) { return clampAdcValue((float)raw + sys.calSoilOffset); }
float calibrationApplyWater(int raw) { return clampAdcValue((float)raw + sys.calWaterOffset); }

static bool validOffset(float offset, float min, float max) {
  return isfinite(offset) && offset >= min && offset <= max;
}

bool calibrationSetOffset(const char* sensor, float offset) {
  if (!sensor || !isfinite(offset)) return false;
  if (strcmp(sensor, "temp") == 0) {
    if (!validOffset(offset, -20.0f, 20.0f)) return false;
    sys.calTempOffset = offset;
    return true;
  }
  if (strcmp(sensor, "hum") == 0) {
    if (!validOffset(offset, -50.0f, 50.0f)) return false;
    sys.calHumOffset = offset;
    return true;
  }
  if (strcmp(sensor, "light") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calLightOffset = (int)offset;
    return true;
  }
  if (strcmp(sensor, "soil") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calSoilOffset = (int)offset;
    return true;
  }
  if (strcmp(sensor, "water") == 0) {
    if (!validOffset(offset, -4095.0f, 4095.0f)) return false;
    sys.calWaterOffset = (int)offset;
    return true;
  }
  return false;
}

// Bản chụp giá trị đã ghi flash. Mỗi put* là một lần commit và NVS chỉ chịu
// được số lần ghi hữu hạn; /calibrate có thể bấm lặp lại cùng một giá trị (trang
// web gọi lại lệnh cũ), nên chỉ ghi các khóa THỰC SỰ đổi — cùng kiểu hỏng mà
// settings.cpp đã sửa.
namespace {
struct CalibSnapshot {
  bool valid;
  float tOff, tGain, hOff, hGain;
  int lOff, sOff, wOff;
};
CalibSnapshot sLast = {};
}  // namespace

static void captureCalibSnapshot() {
  sLast.valid = true;
  sLast.tOff = sys.calTempOffset;
  sLast.tGain = sys.calTempGain;
  sLast.hOff = sys.calHumOffset;
  sLast.hGain = sys.calHumGain;
  sLast.lOff = sys.calLightOffset;
  sLast.sOff = sys.calSoilOffset;
  sLast.wOff = sys.calWaterOffset;
}

void calibrationSave() {
  const bool writeAll = !sLast.valid;
  const bool changed = writeAll ||
      sLast.tOff != sys.calTempOffset || sLast.tGain != sys.calTempGain ||
      sLast.hOff != sys.calHumOffset || sLast.hGain != sys.calHumGain ||
      sLast.lOff != sys.calLightOffset || sLast.sOff != sys.calSoilOffset ||
      sLast.wOff != sys.calWaterOffset;
  if (!changed) {
    LOG_DEBUG("Calibration unchanged, skip flash write");
    return;
  }

  sPrefs.begin("calib", false);
  if (writeAll || sLast.tOff != sys.calTempOffset) sPrefs.putFloat("tOff", sys.calTempOffset);
  if (writeAll || sLast.tGain != sys.calTempGain) sPrefs.putFloat("tGain", sys.calTempGain);
  if (writeAll || sLast.hOff != sys.calHumOffset) sPrefs.putFloat("hOff", sys.calHumOffset);
  if (writeAll || sLast.hGain != sys.calHumGain) sPrefs.putFloat("hGain", sys.calHumGain);
  if (writeAll || sLast.lOff != sys.calLightOffset) sPrefs.putInt("lOff", sys.calLightOffset);
  if (writeAll || sLast.sOff != sys.calSoilOffset) sPrefs.putInt("sOff", sys.calSoilOffset);
  if (writeAll || sLast.wOff != sys.calWaterOffset) sPrefs.putInt("wOff", sys.calWaterOffset);
  sPrefs.end();
  captureCalibSnapshot();
  LOG_DEBUG("Calibration saved");
}

void calibrationLoad() {
  sPrefs.begin("calib", true);
  sys.calTempOffset = sPrefs.getFloat("tOff", 0.0f);
  if (!validOffset(sys.calTempOffset, -20.0f, 20.0f)) sys.calTempOffset = 0.0f;
  sys.calTempGain = sPrefs.getFloat("tGain", 1.0f);
  if (!isfinite(sys.calTempGain) || sys.calTempGain < 0.1f || sys.calTempGain > 10.0f) sys.calTempGain = 1.0f;
  sys.calHumOffset = sPrefs.getFloat("hOff", 0.0f);
  if (!validOffset(sys.calHumOffset, -50.0f, 50.0f)) sys.calHumOffset = 0.0f;
  sys.calHumGain = sPrefs.getFloat("hGain", 1.0f);
  if (!isfinite(sys.calHumGain) || sys.calHumGain < 0.1f || sys.calHumGain > 10.0f) sys.calHumGain = 1.0f;
  sys.calLightOffset = constrain(sPrefs.getInt("lOff", 0), -4095, 4095);
  sys.calSoilOffset = constrain(sPrefs.getInt("sOff", 0), -4095, 4095);
  sys.calWaterOffset = constrain(sPrefs.getInt("wOff", 0), -4095, 4095);
  sPrefs.end();
  captureCalibSnapshot();   // mốc so sánh cho calibrationSave() kế tiếp
}

#ifdef SMARTFARM_TEST_HOOKS
// Chỉ dùng cho bộ test: mock xóa flash giữa các test, bản chụp phải quên theo.
void testResetCalibStatics() { sLast.valid = false; }
#endif
