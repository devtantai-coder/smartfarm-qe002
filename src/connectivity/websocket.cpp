#include "../websocket.h"
#include "../globals.h"
#include "../logger.h"
#include "../config.h"

static char sLastMsg[128] = "";
static unsigned long sBroadcasts = 0;

#if WEBSOCKET_ENABLED && !defined(SMARTFARM_TEST_HOOKS)
#include <WebSocketsServer.h>
static WebSocketsServer sWs(81);
#endif

void websocketInit() {
#if WEBSOCKET_ENABLED
  LOG_INFO("WebSocket init");
#ifndef SMARTFARM_TEST_HOOKS
  sWs.begin();
#endif
#endif
}

void websocketLoop() {
#ifndef SMARTFARM_TEST_HOOKS
#if WEBSOCKET_ENABLED
  sWs.loop();
#endif
#endif
}

void websocketBroadcast(const char* msg) {
  if (!msg) return;
  strncpy(sLastMsg, msg, sizeof(sLastMsg) - 1);
  sLastMsg[sizeof(sLastMsg) - 1] = '\0';
  sBroadcasts++;
#ifndef SMARTFARM_TEST_HOOKS
#if WEBSOCKET_ENABLED
  sWs.broadcastTXT(msg);
#endif
#endif
}

unsigned long websocketBroadcastCount() { return sBroadcasts; }
