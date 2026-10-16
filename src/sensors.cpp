#include "globals.h"
#include "logger.h"
#include "sensors.h"

// ── Moving average buffers ──
static float sTempBuf[SENSOR_FILTER_SIZE] = {0};
static float sHumBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sLightBuf[SENSOR_FILTER_SIZE] = {0};
static int   sSoilBuf[SENSOR_FILTER_SIZE]  = {0};
static int   sWaterBuf[SENSOR_FILTER_SIZE] = {0};
static uint8_t sBufIdx = 0;
static bool sBufFilled = false;

static float avg(float* buf, uint8_t n) {
  float sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += buf[i];
  return sum / n;
}

static int avgInt(int* buf, uint8_t n) {
  long sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += buf[i];
  return (int)(sum / n);
}

float readTemperature() { return dht.readTemperature(); }
float readHumidity()    { return dht.readHumidity(); }
int   readLight()       { return analogRead(LDR_PIN); }
int   readSoil()        { return analogRead(SOIL_PIN); }
int   readWater()       { return analogRead(WATER_PIN); }
bool  readTouch()       { return digitalRead(TOUCH_PIN) == HIGH; }
bool  readPIR()         { return digitalRead(PIR_PIN) == HIGH; }

void readAllSensors() {
  float t = readTemperature();
  float h = readHumidity();

  // Check NaN BEFORE replacing with old values
  bool tIsNan = isnan(t);
  bool hIsNan = isnan(h);

  // Handle NaN from DHT: replace with last known good value
  if (tIsNan) {
    t = sBufFilled ? sTempBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
    if (++sensorFailCount > 5 && sensorHealthy) {
      sensorHealthy = false;
      LOG_ERROR_CODE(501, "DHT11 sensor failing repeatedly");
    }
  }
  if (hIsNan) {
    h = sBufFilled ? sHumBuf[(sBufIdx + SENSOR_FILTER_SIZE - 1) % SENSOR_FILTER_SIZE] : 0;
  }

  // Reset fail count on success (use original NaN check, not replaced values)
  if (!tIsNan && !hIsNan) {
    sensorFailCount = 0;
    sensorHealthy = true;
  }

  int l = readLight();
  int s = readSoil();
  int w = readWater();

  // Push to circular buffer
  sTempBuf[sBufIdx] = t;
  sHumBuf[sBufIdx]  = h;
  sLightBuf[sBufIdx] = l;
  sSoilBuf[sBufIdx]  = s;
  sWaterBuf[sBufIdx] = w;
  sBufIdx = (sBufIdx + 1) % SENSOR_FILTER_SIZE;
  if (sBufIdx == 0) sBufFilled = true;

  uint8_t n = sBufFilled ? SENSOR_FILTER_SIZE : sBufIdx;
  if (n == 0) n = 1;

  gTemp  = avg(sTempBuf, n);
  gHum   = avg(sHumBuf, n);
  gLight = avgInt(sLightBuf, n);
  gSoil  = avgInt(sSoilBuf, n);
  gWater = avgInt(sWaterBuf, n);
  gPir   = readPIR();
  gTouch = readTouch();
}

// ── Simple weather guess from light + humidity (activity 11) ──
Weather weatherStatus() {
  bool bright = gLight > (lightThreshold + 50);
  bool dark = gLight < (lightThreshold - 50);
  bool humid = gHum > 80.0f;
  if (bright) return humid ? WEATHER_CLOUDY : WEATHER_SUNNY;
  if (dark)   return humid ? WEATHER_RAIN : WEATHER_CLOUDY;
  return WEATHER_NORMAL;
}
