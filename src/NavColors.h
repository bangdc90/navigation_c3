#ifndef NAV_COLORS_H
#define NAV_COLORS_H

#include <Arduino.h>

// Định nghĩa các màu sắc dùng cho Navigation UI
namespace NavColors {
    // Màu sắc chính cho giao diện
    static constexpr uint16_t Background = 0x0000;  // Đen
    static constexpr uint16_t Foreground = 0xFFFF;  // Trắng
    static constexpr uint16_t Accent = 0x07E0;      // Xanh lá (như trong navigation icon)
    static constexpr uint16_t Secondary = 0x001F;   // Xanh dương
    static constexpr uint16_t Attention = 0xF800;   // Đỏ
    static constexpr uint16_t Info = 0xFFE0;        // Vàng
}

#endif // NAV_COLORS_H
