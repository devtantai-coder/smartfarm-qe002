#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL };

// Minimum level printed on Serial (change gMinLogLevel in logger.cpp)
extern LogLevel gMinLogLevel;

// Per-module debug flags
extern bool dbgSensors, dbgActuators, dbgControl;
extern bool dbgWebserver, dbgWifi, dbgOled;
extern bool dbgSettings, dbgLogger, dbgRgb;

void logMessage(LogLevel level, const char* func, int line, const char* msg);
void logError(LogLevel level, const char* func, int line, int code, const char* msg);

// Debug module functions
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

// Module-specific debug (only prints if module flag is on)
#define MOD_DEBUG(mod, msg)   do { if (dbg##mod) logMessage(LOG_DEBUG, __func__, __LINE__, msg); } while(0)

#endif
