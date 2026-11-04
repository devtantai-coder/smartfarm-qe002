#ifndef SENSORS_H
#define SENSORS_H

// Đoán thời tiết từ ánh sáng/độ ẩm (bài 11)
enum Weather { WEATHER_NORMAL, WEATHER_SUNNY, WEATHER_CLOUDY, WEATHER_RAIN };

void readAllSensors();
float readTemperature();
float readHumidity();
int   readLight();
int   readSoil();
int   readWater();
bool  readTouch();
bool  readPIR();
Weather weatherStatus();

#endif
