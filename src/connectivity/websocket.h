#ifndef SMARTFARM_WEBSOCKET_H
#define SMARTFARM_WEBSOCKET_H
#include <Arduino.h>

void websocketInit();
void websocketLoop();
void websocketBroadcast(const char* msg);
unsigned long websocketBroadcastCount();

// Số client đang kết nối (host test trả 0).
unsigned websocketClientCount();

#endif
