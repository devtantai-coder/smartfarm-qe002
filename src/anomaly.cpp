#include "anomaly.h"
#include "globals.h"
#include <cmath>

void anomalyInit() {}
void anomalyUpdate() {}
float anomalyZScore(float value, float mean, float stdDev) {
  if (stdDev == 0.0f) return 0.0f;
  return (value - mean) / stdDev;
}
bool anomalyIsHealthy(const char* sensor) {
  if (!sensor) return true;
  return true;
}
float anomalyScore(const char* sensor) { (void)sensor; return 0.0f; }
