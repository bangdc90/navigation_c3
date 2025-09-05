#ifndef VIETNAMESE_FONTS_H
#define VIETNAMESE_FONTS_H

#include <Arduino.h>
#include <lvgl.h>          // Include LVGL wrapper first
#include "fonts/local_fonts.h"

// Các màu sắc cho UI
namespace NavColors {
    static const uint32_t BackgroundHex = 0x000000;  // Đen
    static const uint32_t ForegroundHex = 0xFFFFFF;  // Trắng
    static const uint32_t AccentHex = 0x00FF00;      // Xanh lá
    static const uint32_t SecondaryHex = 0x0000FF;   // Xanh dương
    static const uint32_t AttentionHex = 0xFF0000;   // Đỏ
    static const uint32_t InfoHex = 0xFFFF00;        // Vàng
}

// Lớp quản lý font tiếng Việt
class VietnameseFonts {
private:
    static lv_font_t* _normalFont;
    static lv_font_t* _boldFont;
    static lv_font_t* _semiboldFont;
    static lv_font_t* _numberBoldFont;
    
public:
    // Khởi tạo các font
    static void init() {
        // Sử dụng các font đã được tạo với đầy đủ ký tự tiếng Việt
        _normalFont = get_montserrat_24();
        _boldFont = get_montserrat_bold_32();
        _semiboldFont = get_montserrat_semibold_28();
        _numberBoldFont = get_montserrat_number_bold_48();
        
        // In thông tin khởi tạo font
        Serial.println("Vietnamese fonts initialized");
        Serial.println("Using UTF-8 encoding for text");
    }
    
    // Lấy font thường
    static lv_font_t* getNormalFont() {
        return _normalFont;
    }
    
    // Lấy font đậm
    static lv_font_t* getBoldFont() {
        return _boldFont;
    }
    
    // Lấy font semi-bold
    static lv_font_t* getSemiboldFont() {
        return _semiboldFont;
    }
    
    // Lấy font số đậm
    static lv_font_t* getNumberBoldFont() {
        return _numberBoldFont;
    }
    
    // Tạo đối tượng text LVGL với font tiếng Việt
    static lv_obj_t* createText(lv_obj_t* parent, const char* text, lv_font_t* font, lv_color_t color, lv_align_t align, lv_coord_t x, lv_coord_t y) {
        lv_obj_t* label = lv_label_create(parent);
        
        // Thiết lập text mode để xử lý UTF-8
        lv_label_set_text(label, text);
        
        // Đảm bảo sử dụng font Montserrat với đầy đủ ký tự Việt
        lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
        
        // Đặt vị trí label
        lv_obj_align(label, align, x, y);
        
        return label;
    }
};

// Định nghĩa các biến tĩnh
lv_font_t* VietnameseFonts::_normalFont = nullptr;
lv_font_t* VietnameseFonts::_boldFont = nullptr;
lv_font_t* VietnameseFonts::_semiboldFont = nullptr;
lv_font_t* VietnameseFonts::_numberBoldFont = nullptr;

#endif // VIETNAMESE_FONTS_H
