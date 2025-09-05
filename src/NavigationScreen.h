#ifndef NAVIGATION_SCREEN_H
#define NAVIGATION_SCREEN_H

#include <Arduino.h>
#include "LGFX_Config.h"  // Cấu hình LGFX
#include "ChronosESP32Patched.h" // Để lấy thông tin Navigation
#include "NavColors.h" // Định nghĩa màu sắc cho Navigation UI

// Kích thước dữ liệu biểu tượng chỉ đường từ Chronos app
#define ICON_DATA_SIZE 288 // 48x48 pixels, 1 bit mỗi pixel = 48*48/8 = 288 bytes

// Icon đồng hồ (16x16)
const unsigned char CLOCK_ICON[] PROGMEM = {
    0x00, 0x00, 0x03, 0xC0, 0x0C, 0x30, 0x10, 0x08, 0x20, 0x04, 0x40, 0x02,
    0x40, 0x02, 0x40, 0x02, 0x42, 0x42, 0x42, 0x42, 0x40, 0x02, 0x40, 0x02,
    0x20, 0x04, 0x10, 0x08, 0x0C, 0x30, 0x03, 0xC0
};

// Icon điểm đến (16x16)
const unsigned char PIN_ICON[] PROGMEM = {
    0x01, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x07, 0xE0, 0x0F, 0xF0, 0x0F, 0xF0,
    0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x07, 0xE0, 0x07, 0xE0, 0x03, 0xC0,
    0x03, 0xC0, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00
};

// Lớp hiển thị màn hình điều hướng cải tiến - thiết kế theo mẫu Apple Watch
class NavigationScreen {
private:
    LGFX_Device &_lcd;
    Navigation _navData;
    ChronosESP32Patched* _chronos; // Thay đổi từ ESP32Time* sang ChronosESP32Patched*
    uint8_t* _iconData; 
    bool _hasValidIcon;
    uint32_t _iconCRC;
    
    // Màu sắc cho giao diện
    static constexpr uint16_t COLOR_BLACK = 0x0000;
    static constexpr uint16_t COLOR_WHITE = 0xFFFF;
    static constexpr uint16_t COLOR_RED = 0xF800;
    static constexpr uint16_t COLOR_GREEN = 0x07E0;
    static constexpr uint16_t COLOR_BLUE = 0x001F;
    static constexpr uint16_t COLOR_CYAN = 0x07FF;
    static constexpr uint16_t COLOR_MAGENTA = 0xF81F;
    static constexpr uint16_t COLOR_YELLOW = 0xFFE0;
    static constexpr uint16_t COLOR_GRAY = 0x8410;
    static constexpr uint16_t COLOR_DARKGRAY = 0x4208;
    
    // Phương thức vẽ navigation icon (mũi tên chỉ hướng)
    void drawNavigationIcon(int x, int y) {
        if (!_hasValidIcon || _iconData == nullptr) {
            // Vẽ icon mặc định nếu không có icon
            _lcd.fillRoundRect(x-24, y-24, 48, 48, 5, COLOR_GREEN);
            _lcd.fillTriangle(x, y-15, x+15, y+10, x-15, y+10, COLOR_BLACK);
            return;
        }
        
        // Vẽ icon từ dữ liệu bitmap nhận được (48x48 pixels, monochrome)
        _lcd.startWrite();
        for (int row = 0; row < 48; row++) {
            for (int col = 0; col < 48; col++) {
                int byteIndex = (row * 48 + col) / 8;
                int bitPos = 7 - ((row * 48 + col) % 8); // Bit MSB first
                
                if (byteIndex < ICON_DATA_SIZE) {
                    bool pixelOn = (_iconData[byteIndex] & (1 << bitPos)) != 0;
                    if (pixelOn) {
                        _lcd.writePixel(x-24+col, y-24+row, COLOR_GREEN);
                    }
                }
            }
        }
        _lcd.endWrite();
    }
    
    // Phương thức vẽ icon 16x16
    void drawIcon(int x, int y, const unsigned char* icon, uint16_t color) {
        for (int row = 0; row < 16; row++) {
            for (int col = 0; col < 16; col++) {
                int byteIndex = row * 2 + (col / 8);
                int bitPos = 7 - (col % 8);
                
                bool pixelOn = (icon[byteIndex] & (1 << bitPos)) != 0;
                if (pixelOn) {
                    _lcd.drawPixel(x + col, y + row, color);
                }
            }
        }
    }
    
public:
    NavigationScreen(LGFX_Device &lcd, ChronosESP32Patched* chronos = nullptr) 
        : _lcd(lcd), _chronos(chronos), _hasValidIcon(false), _iconData(nullptr), _iconCRC(0) {
    }
    
    // Cập nhật đối tượng Chronos
    void setChronos(ChronosESP32Patched* chronos) {
        _chronos = chronos;
    }
    
    // Cập nhật dữ liệu điều hướng
    void updateNavigation(const Navigation &navData) {
        _navData = navData;
        
        // Cập nhật trạng thái icon
        _hasValidIcon = navData.hasIcon;
        
        // Nếu icon đã thay đổi, cập nhật dữ liệu
        if (_hasValidIcon && _iconCRC != navData.iconCRC) {
            _iconData = const_cast<uint8_t*>(navData.icon);
            _iconCRC = navData.iconCRC;
        }
    }
    
    // Hiển thị màn hình điều hướng theo thiết kế Apple Watch
    void display() {
        _lcd.fillScreen(COLOR_BLACK);
        
        // Header - Thanh trạng thái
        drawHeader();
        
        // Body - Hướng dẫn chỉ đường
        drawDirectionContent();
        
        _lcd.display(); // Cập nhật màn hình
    }
    
    // Vẽ phần header (thanh trạng thái)
    void drawHeader() {
        // 1. Hiển thị ETA ở góc trên bên trái
        String etaTime = _navData.eta;
        // Cải thiện xử lý ETA để loại bỏ chữ "kiến" và chỉ lấy thời gian
        if (etaTime.indexOf(":") > 0) {
            int colonPos = etaTime.indexOf(":");
            int lastSpacePos = -1;
            for (int i = 0; i < colonPos; i++) {
                if (etaTime.charAt(i) == ' ') {
                    lastSpacePos = i;
                }
            }
            
            if (lastSpacePos >= 0) {
                etaTime = etaTime.substring(lastSpacePos + 1);
            }
            
            int secondColon = etaTime.indexOf(":", colonPos + 1);
            if (secondColon > 0) {
                etaTime = etaTime.substring(0, secondColon);
            }
        }
        
        _lcd.setFont(nullptr); // Dùng font mặc định
        _lcd.setTextSize(1);
        _lcd.setTextColor(COLOR_WHITE);
        
        // Định dạng "4:30 PM ETA"
        String formattedETA = etaTime;
        if (etaTime.indexOf(":") > 0) {
            int hour = etaTime.substring(0, etaTime.indexOf(":")).toInt();
            String ampm = (hour >= 12) ? "PM" : "AM";
            if (hour > 12) hour -= 12;
            if (hour == 0) hour = 12;
            formattedETA = String(hour) + etaTime.substring(etaTime.indexOf(":")) + " " + ampm + " ETA";
        } else {
            formattedETA = etaTime + " ETA";
        }
        
        _lcd.setCursor(5, 15);
        _lcd.print(formattedETA);
        
        // 2. Hiển thị thời gian hiện tại ở góc trên bên phải
        String currentTime;
        if (_chronos != nullptr) {
            int hour = _chronos->getHour();
            int min = _chronos->getMinute();
            // Định dạng thời gian hiện tại "4:04" (không có AM/PM)
            currentTime = String(hour) + ":" + (min < 10 ? "0" : "") + String(min);
        } else {
            currentTime = "00:00"; // Giá trị mặc định nếu không có Chronos
        }
        
        int timeWidth = currentTime.length() * 6; // Ước tính 6 pixel mỗi ký tự với textSize 1
        _lcd.setCursor(240 - timeWidth - 5, 15);
        _lcd.print(currentTime);
    }
    
    // Vẽ phần nội dung hướng dẫn (body)
    void drawDirectionContent() {
        _lcd.setFont(nullptr); // Dùng font mặc định cho UI giống Apple Watch
        
        // 1. Vẽ mũi tên chỉ hướng (sử dụng bitmap được gửi từ Chronos app)
        if (_hasValidIcon && _iconData != nullptr) {
            // Sử dụng icon từ Chronos app nếu có
            // Vẽ icon ở vị trí tương tự như Apple Watch
            _lcd.startWrite();
            for (int row = 0; row < 48; row++) {
                for (int col = 0; col < 48; col++) {
                    int byteIndex = (row * 48 + col) / 8;
                    int bitPos = 7 - ((row * 48 + col) % 8); // Bit MSB first
                    
                    if (byteIndex < ICON_DATA_SIZE) {
                        bool pixelOn = (_iconData[byteIndex] & (1 << bitPos)) != 0;
                        if (pixelOn) {
                            _lcd.writePixel(40 + col, 65 + row, NavColors::AccentHex);
                        }
                    }
                }
            }
            _lcd.endWrite();
        } else {
            // Vẽ mũi tên mặc định nếu không có icon từ app
            _lcd.fillRoundRect(40, 65, 48, 48, 5, NavColors::AccentHex);
            _lcd.fillTriangle(64, 75, 75, 100, 53, 100, COLOR_BLACK);
        }
        
        // 2. Hiển thị khoảng cách (50 m) bên phải mũi tên
        String distanceText = _navData.distance;
        _lcd.setTextColor(COLOR_WHITE);
        _lcd.setTextSize(2);
        _lcd.setCursor(140, 95);
        _lcd.print(distanceText);
        
        // 3. Hiển thị hướng dẫn chỉ đường
        // "Turn left onto Bà Huyện Thanh Quan"
        String directionText = _navData.directions;
        
        _lcd.setTextColor(COLOR_WHITE);
        _lcd.setTextSize(1);
        
        // Chia thành các dòng nếu quá dài
        if (directionText.length() > 28) {
            int spacePos = -1;
            for (int i = 20; i < min(28, (int)directionText.length()); i++) {
                if (directionText.charAt(i) == ' ') {
                    spacePos = i;
                    break;
                }
            }
            
            if (spacePos > 0) {
                String line1 = directionText.substring(0, spacePos);
                String line2 = directionText.substring(spacePos + 1);
                
                // Vẽ dòng 1
                int textWidth1 = line1.length() * 6;
                _lcd.setCursor(120 - textWidth1/2, 150);
                _lcd.print(line1);
                
                // Vẽ dòng 2
                int textWidth2 = line2.length() * 6;
                _lcd.setCursor(120 - textWidth2/2, 170);
                _lcd.print(line2);
            } else {
                // Nếu không tìm thấy khoảng trắng, cắt ngắn và thêm "..."
                String shortenedText = directionText.substring(0, 25) + "...";
                int textWidth = shortenedText.length() * 6;
                _lcd.setCursor(120 - textWidth/2, 160);
                _lcd.print(shortenedText);
            }
        } else {
            // Vẽ trên một dòng nếu đủ ngắn
            int textWidth = directionText.length() * 6;
            _lcd.setCursor(120 - textWidth/2, 160);
            _lcd.print(directionText);
        }
    }
    
    // Vẽ icon tùy chỉnh
    void drawCustomIcon(int x, int y, const unsigned char* icon, uint16_t color, int width, int height) {
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                int byteIndex = (row * width + col) / 8;
                int bitPos = 7 - ((row * width + col) % 8);
                
                bool pixelOn = (icon[byteIndex] & (1 << bitPos)) != 0;
                if (pixelOn) {
                    _lcd.drawPixel(x + col, y + row, color);
                }
            }
        }
    }
};

#endif // NAVIGATION_SCREEN_H
