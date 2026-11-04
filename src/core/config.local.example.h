// Ví dụ cấu hình cục bộ — KHÔNG commit file này.
// Sao chép thành src/core/config.local.h rồi điền credential thật.
// File src/core/config.local.h đã nằm trong .gitignore.
//
// Cách khác (không cần file này): truyền build flag khi nạp, ví dụ Arduino IDE
// hoặc platformio extra_build_flags:
//   -DDEFAULT_SSID="TenWiFi" -DDEFAULT_PASSWORD="MatKhauWiFi"
//   -DWEB_TOKEN="mat-khau-web" -DOTA_PASSWORD="mat-khau-ota"

// #define DEFAULT_SSID "TenWiFiCuaBan"
// #define DEFAULT_PASSWORD "MatKhauWiFiCuaBan"
// #define WEB_TOKEN "mat-khau-web-loai-cua-ban"
// #define OTA_PASSWORD "mat-khau-ota-cua-ban"
