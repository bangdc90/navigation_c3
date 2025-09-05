#ifndef NAV_ICON_RENDERER_H
#define NAV_ICON_RENDERER_H

#include <Arduino.h>
#include "ChronosESP32Patched.h"
#include "LGFX_Config.h"  // Giả sử bạn đã có cấu hình LGFX

class NavIconRenderer {
public:
    // Hiển thị icon điều hướng từ dữ liệu thô của Chronos
    static void renderNavigationIcon(LGFX& display, const uint8_t* iconData, int16_t x, int16_t y, uint16_t fgColor = TFT_WHITE, uint16_t bgColor = TFT_BLACK) {
        // Kích thước icon mặc định là 48x48 pixels
        const int16_t width = 48;
        const int16_t height = 48;
        
        display.startWrite();
        
        // Vẽ từng pixel của bitmap
        for (int16_t j = 0; j < height; j++) {
            for (int16_t i = 0; i < width; i++) {
                // Tính vị trí byte và bit
                int16_t byteIndex = (j * width + i) / 8;
                int16_t bitIndex = 7 - ((j * width + i) % 8); // MSB first
                
                // Lấy giá trị bit
                bool pixel = (iconData[byteIndex] & (1 << bitIndex)) != 0;
                
                // Vẽ pixel
                display.writePixel(x + i, y + j, pixel ? fgColor : bgColor);
            }
        }
        
        display.endWrite();
    }
    
    // Hiển thị icon điều hướng từ đối tượng Navigation
    static void renderNavigationIcon(LGFX& display, const Navigation& navigation, int16_t x, int16_t y, uint16_t fgColor = TFT_WHITE, uint16_t bgColor = TFT_BLACK) {
        if (!navigation.hasIcon) {
            Serial.println("No navigation icon available");
            return;
        }
        
        renderNavigationIcon(display, navigation.icon, x, y, fgColor, bgColor);
    }
    
    // Debug: In icon dưới dạng ASCII art ra Serial
    static void debugPrintIcon(const uint8_t* iconData) {
        const int16_t width = 48;
        const int16_t height = 48;
        
        Serial.println("\nNavigation Icon (ASCII Art):");
        
        for (int16_t j = 0; j < height; j++) {
            String line;
            for (int16_t i = 0; i < width; i++) {
                int16_t byteIndex = (j * width + i) / 8;
                int16_t bitIndex = 7 - ((j * width + i) % 8); // MSB first
                
                bool pixel = (iconData[byteIndex] & (1 << bitIndex)) != 0;
                line += pixel ? "#" : ".";
            }
            Serial.println(line);
        }
    }
    
    // Debug: In thông tin về icon ra Serial
    static void debugIconInfo(const Navigation& navigation) {
        Serial.println("\n===== NAVIGATION ICON INFO =====");
        Serial.printf("Has Icon: %s\n", navigation.hasIcon ? "true" : "false");
        Serial.printf("Icon CRC: 0x%08X\n", navigation.iconCRC);
        
        if (navigation.hasIcon) {
            // In các byte đầu tiên của icon để kiểm tra
            Serial.println("Icon Data (first 32 bytes):");
            for (int i = 0; i < 32; i++) {
                Serial.printf("%02X ", navigation.icon[i]);
                if ((i + 1) % 16 == 0) Serial.println();
            }
            Serial.println();
            
            // In icon dưới dạng ASCII art
            debugPrintIcon(navigation.icon);
        }
        
        Serial.println("==================================");
    }
};

#endif // NAV_ICON_RENDERER_H
