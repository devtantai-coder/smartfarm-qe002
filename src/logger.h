#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL };

// Mức tối thiểu được in ra Serial (đổi gMinLogLevel trong logger.cpp)
extern LogLevel gMinLogLevel;

// Các cờ debug theo từng module
extern bool dbgSensors, dbgActuators, dbgControl;
extern bool dbgWebserver, dbgWifi, dbgOled;
extern bool dbgSettings, dbgLogger, dbgRgb;

void logMessage(LogLevel level, const char* func, int line, const char* msg);
void logError(LogLevel level, const char* func, int line, int code, const char* msg);

// Dòng tóm tắt cảm biến cho Serial Monitor (activity 7: dữ liệu đất qua serial).
// Chỉ in khi mức log tối thiểu là DEBUG.
void logSensors();

// Các hàm debug của module
void debugSensors();
void debugActuators();
void debugControl();
void debugWebserver();
void debugWifi();
void debugOled();
void debugSettings();
void debugLogger();

#define LOG_DEBUG(msg)  logMessage(LOG_DEBUG,  __func__, __LINE__, msg)
#define LOG_INFO(msg)   logMessage(LOG_INFO,   __func__, __LINE__, msg)
#define LOG_WARN(msg)   logMessage(LOG_WARNING,__func__, __LINE__, msg)
#define LOG_ERROR(msg)  logMessage(LOG_ERROR,  __func__, __LINE__, msg)
#define LOG_FATAL(msg)  logMessage(LOG_FATAL,  __func__, __LINE__, msg)
#define LOG_ERROR_CODE(code, msg) logError(LOG_ERROR, __func__, __LINE__, code, msg)

// Debug riêng theo module (chỉ in khi cờ module đang bật)
#define MOD_DEBUG(mod, msg)   do { if (dbg##mod) logMessage(LOG_DEBUG, __func__, __LINE__, msg); } while(0)

#endif
