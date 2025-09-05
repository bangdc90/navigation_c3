#ifndef CHRONOS_TYPES_H
#define CHRONOS_TYPES_H

#include <Arduino.h>

// Cấu trúc dữ liệu điều hướng
struct AppNavigation {
    bool active = false;
    bool hasIcon = false;
    bool isNavigation = false;
    uint32_t iconCRC = 0;
    String eta = "";         // Thời gian đến nơi
    String title = "";       // Tên đường/địa điểm hiện tại
    String duration = "";    // Thời gian còn lại của cuộc hành trình
    String distance = "";    // Khoảng cách đến đích
    String speed = "";       // Tốc độ hiện tại
    String directions = "";  // Hướng dẫn rẽ tiếp theo
    uint8_t icon[384];       // 96*4 bytes for icon data
    
    // Constructor mặc định
    AppNavigation() {
        active = false;
        hasIcon = false;
        isNavigation = false;
        iconCRC = 0xFFFFFFFF;
        eta = "Navigation";
        title = "Bật Google Maps trên phone";
        duration = "";
        distance = "";
        speed = "";
        directions = "Start navigation on Google maps";
        memset(icon, 0, sizeof(icon));
    }
};

// Enums for configuration types
enum class AppConfigType {
    CF_TIME,
    CF_NAV_DATA,
    CF_NAV_ICON,
    CF_PBAT,
    CF_ALARM,
    CF_USER,
    CF_SED,
    CF_QUIET,
    CF_RTW,
    CF_HOURLY,
    CF_CAMERA,
    CF_LANG,
    CF_HR24,
    CF_SLEEP,
    CF_APP,
    CF_CONTACT,
    CF_QR,
    CF_FONT,
    CF_RST
};

#endif // CHRONOS_TYPES_H
