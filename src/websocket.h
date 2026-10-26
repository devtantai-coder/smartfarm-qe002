#ifndef SMARTFARM_WEBSOCKET_H
#define SMARTFARM_WEBSOCKET_H
#include <Arduino.h>
void websocketInit();
void websocketLoop();
void websocketBroadcast(const char* msg);
unsigned long websocketBroadcastCount();
#endif
