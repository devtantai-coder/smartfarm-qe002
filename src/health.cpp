#include "health.h"
#include "globals.h"
bool healthCheck() { return true; }
const char* healthStatus() { return "ok"; }
void handleHealth() {
  server.send(200, "text/plain", healthStatus());
}
