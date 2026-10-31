#ifndef WIFI_H
#define WIFI_H

void manageWiFi();
// Chỉ loopTask được dùng bản cache: sLastStatus/sStatusAt/sStatusValid không có
// khóa, task nền chạm vào là gây race (và làm manageWiFi đọc phải giá trị do
// task khác ghi).
bool wifiConnected();

// Bản hỏi thẳng driver, dành cho tác vụ mạng nền (networker). Không đụng cache.
bool wifiConnectedFresh();

// Chuỗi IP hiện tại (rỗng khi chưa kết nối). Cache theo WIFI_STATUS_POLL_MS
// để webserver/OLED không phải hỏi driver WiFi trên mỗi request/lần vẽ.
const char* wifiIpString();

#endif
