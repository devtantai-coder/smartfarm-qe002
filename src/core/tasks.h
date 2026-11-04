#ifndef SMARTFARM_TASKS_H
#define SMARTFARM_TASKS_H

#include <Arduino.h>

void sensorTaskFn(void*);
void controlTaskFn(void*);
void commTaskFn(void*);
void displayTaskFn(void*);
void powerTaskFn(void*);

#ifdef SMARTFARM_TEST_HOOKS
void runExtras();
#endif

#endif
