#ifndef SMARTFARM_NOTIFY_H
#define SMARTFARM_NOTIFY_H

#include <Arduino.h>

void notifyInit();
bool notifyEvent(const char* kind, const char* message);
bool notifyEnabled();

// ── Dùng bởi tác vụ mạng nền (src/connectivity/networker.cpp) ──
// notifyEvent() chỉ xếp job vào queue; phần gửi thật chạy trên task nền.
bool notifySendNow(const char* kind, const char* message);
// Báo kết quả một lần gửi: thất bại thì mở lại cửa sổ chống spam sớm hơn.
void notifyComplete(const char* kind, bool sent);

#ifdef SMARTFARM_TEST_HOOKS
void testResetNotifyStatics();
void testNotifyForceEnabled(bool on);
const char* testNotifyLastEvent();
const char* testNotifyLastMessage();
#endif

#endif
