#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Shared configuration settings used by multiple files
namespace Config {
  // Navigation settings
  constexpr bool NAVIGATION_ENABLED = true;  // Bật/tắt tính năng chỉ đường
  constexpr uint16_t NAV_UPDATE_INTERVAL = 1000; // Thời gian cập nhật thông tin chỉ đường (ms)
  constexpr uint16_t NAV_ALERT_DISTANCE = 100;   // Khoảng cách cảnh báo rẽ (mét)
}

#endif // CONFIG_H
