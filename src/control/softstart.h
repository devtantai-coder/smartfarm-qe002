#ifndef SMARTFARM_SOFTSTART_H
#define SMARTFARM_SOFTSTART_H
#include <Arduino.h>
void softStartInit();
void softStartUpdate();
void softStartPump(bool on);
bool softStartIsActive();
#endif
