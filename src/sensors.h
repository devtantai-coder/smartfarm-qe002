#ifndef SENSORS_H
#define SENSORS_H

void readAllSensors();
float readTemperature();
float readHumidity();
int   readLight();
int   readSoil();
int   readWater();
bool  readTouch();
bool  readPIR();

#endif
