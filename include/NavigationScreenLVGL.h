#ifndef NAVIGATION_SCREEN_LVGL_H
#define NAVIGATION_SCREEN_LVGL_H

#include <Arduino.h>
#include <lvgl.h>
#include "LVGL_Config.h"
#include "VietnameseFonts.h"
#include "ChronosTypes.h"
#include "ChronosManager.h"
#include "ESP32Time.h"
#include "bg.h" // Thêm include để sử dụng hình nền

// Kích thước dữ liệu biểu tượng chỉ đường từ Chronos app
#define ICON_DATA_SIZE 288 // 48x48 pixels, 1 bit mỗi pixel = 48*48/8 = 288 bytes

// Hàm chuyển đổi bitmap 1-bit sang RGB565
void convert1BitBitmapToRgb565(void* dst, const void* src, uint16_t width, uint16_t height, uint16_t color, uint16_t bgColor, bool invert = false) {
    uint16_t* d      = (uint16_t*)dst;
    const uint8_t* s = (const uint8_t*)src;

    auto activeColor   = invert ? bgColor : color;
    auto inactiveColor = invert ? color : bgColor;

    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            if (s[(y * width + x) / 8] & (1 << (7 - x % 8))) {
                d[y * width + x] = activeColor;
            } else {
                d[y * width + x] = inactiveColor;
            }
        }
    }
}

// Lớp hiển thị màn hình điều hướng sử dụng LVGL
class NavigationScreenLVGL {
private:
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _navIcon = nullptr;
    lv_obj_t* _directionLabel = nullptr;
    lv_obj_t* _distanceLabel = nullptr;
    lv_obj_t* _titleLabel = nullptr;     // Thay _etaLabel bằng _titleLabel
    lv_obj_t* _timeLabel = nullptr;
    lv_obj_t* _speedLabel = nullptr;     // Thêm label hiển thị tốc độ
    lv_obj_t* _durationLabel = nullptr;  // Thêm label hiển thị thời gian hành trình
    lv_obj_t* _bgImage = nullptr; // Thêm đối tượng cho hình nền
    lv_obj_t* _defaultMessageLabel = nullptr; // Label hiển thị "Start navigation on Google maps" khi không active
    lv_obj_t* _distanceContainer = nullptr; // Container cho distance với nền màu vàng
    
    // Biểu tượng điều hướng
    lv_img_dsc_t _navIconDesc = {};
    uint8_t* _iconData = nullptr;
    uint8_t _iconBuffer[ICON_DATA_SIZE]; // Buffer để lưu trữ bản sao của dữ liệu icon
    bool _hasValidIcon = false;
    uint32_t _iconCRC = 0;
    
    // Dữ liệu điều hướng
    AppNavigation _navData;
    
    // Vẽ trực tiếp bitmap 1-bit bằng API của LVGL
    void drawNavIconDirectly() {
        if (!_hasValidIcon || _iconData == nullptr || _screen == nullptr) {
        //
            return;
        }
        
        // Xóa icon cũ nếu có
        if (_navIcon != nullptr) {
            lv_obj_del(_navIcon);
            _navIcon = nullptr;
        }
        
        // Xóa bộ nhớ dành cho descriptor cũ nếu có
        if (_navIconDesc.data != nullptr && _navIconDesc.data != _iconData) {
            _navIconDesc.data = nullptr;
        }
        
        // Tạo mảng pixel mới ở định dạng TRUE_COLOR
        static lv_color_t pixels[48 * 48];
        
        // Sử dụng hàm convert1BitBitmapToRgb565 để chuyển đổi bitmap
        // Màu đỏ cho bit 1, màu xanh dương cho bit 0 - dễ debug
        uint16_t activeColor = 0xFFFF;   // Trắng
        uint16_t bgColor = 0x0000;       // Đen
        
        // Chuyển đổi bitmap 1-bit sang RGB565 sử dụng hàm mới
        convert1BitBitmapToRgb565(pixels, _iconData, 48, 48, activeColor, bgColor, false);

        // Tạo descriptor từ dữ liệu pixel TRUE_COLOR
        _navIconDesc.header.cf = LV_IMG_CF_TRUE_COLOR;
        _navIconDesc.header.always_zero = 0;
        _navIconDesc.header.reserved = 0;
        _navIconDesc.header.w = 48;
        _navIconDesc.header.h = 48;
        _navIconDesc.data_size = 48 * 48 * sizeof(lv_color_t);
        _navIconDesc.data = (const uint8_t*)pixels;
        
        // Tạo đối tượng hình ảnh
        _navIcon = lv_img_create(_screen);
        
        // Áp dụng nguồn hình ảnh
        lv_img_set_src(_navIcon, &_navIconDesc);
        
        // Thêm hiệu ứng thu phóng để làm nổi bật icon
        lv_img_set_zoom(_navIcon, 256); // 256 = 100% (không thu phóng)

        // Căn lề trái, cách mép trên 50 pixel
        lv_obj_align(_navIcon, LV_ALIGN_TOP_LEFT, 5, 40);
    }

public:
    NavigationScreenLVGL() : _iconData(_iconBuffer), _hasValidIcon(false), _iconCRC(0) {
        // Khởi tạo buffer icon với giá trị 0
        memset(_iconBuffer, 0, ICON_DATA_SIZE);
    }
    
    ~NavigationScreenLVGL() {
        // Không giải phóng bộ nhớ static hoặc con trỏ trực tiếp
        // LVGL sử dụng con trỏ tới dữ liệu pixels trong static buffer
        _navIconDesc.data = nullptr;
        
        // Xóa đối tượng LVGL nếu còn tồn tại
        if (_navIcon) {
            lv_obj_del(_navIcon);
            _navIcon = nullptr;
        }
    }
    
    // Kiểm tra xem screen đã được tạo và sẵn sàng hiển thị chưa
    bool isScreenReady() {
        return _screen != nullptr;
    }
    
    // Cập nhật dữ liệu điều hướng
    void updateNavigation(const AppNavigation &navData) {
        // Kiểm tra trạng thái active trước khi cập nhật
        bool wasActive = _navData.active && _navData.isNavigation;
        bool willBeActive = navData.active && navData.isNavigation;
        
        // Log trạng thái thay đổi
        if (wasActive != willBeActive) {
            Serial.printf("Navigation state changing: %s -> %s\n", 
                         wasActive ? "active" : "inactive", 
                         willBeActive ? "active" : "inactive");
            
            // Nếu chuyển từ active sang inactive, reset hoàn toàn UI
            if (wasActive && !willBeActive) {
                Serial.println("Resetting navigation UI state completely");
                
                // Reset content của các label
                if (_directionLabel) lv_label_set_text(_directionLabel, "");
                if (_distanceLabel) lv_label_set_text(_distanceLabel, "0 km");
                if (_speedLabel) lv_label_set_text(_speedLabel, "");
            }
            
            // Nếu chuyển từ inactive sang active, đảm bảo distance container có kích thước phù hợp
            if (!wasActive && willBeActive) {
                Serial.println("Switching from inactive to active - ensuring container sizes");
                
                // Đảm bảo container được hiển thị trước khi cập nhật kích thước
                lv_obj_clear_flag(_distanceContainer, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(_distanceLabel, LV_OBJ_FLAG_HIDDEN);
                
                // Đặt lại kích thước container theo nội dung
                lv_obj_set_width(_distanceContainer, LV_SIZE_CONTENT);
                lv_obj_update_layout(_distanceContainer);
                lv_obj_center(_distanceLabel);
                
                if(lv_obj_has_flag(_directionLabel, LV_OBJ_FLAG_HIDDEN))
                {
                    lv_obj_clear_flag(_directionLabel, LV_OBJ_FLAG_HIDDEN);
                    // Reset vị trí cuộn text để bắt đầu chạy từ đầu
                    // Đặt lại text để bắt đầu hiệu ứng scroll từ đầu
                    const String currentText = String(lv_label_get_text(_directionLabel));
                    lv_label_set_text(_directionLabel, "");  // Xóa text
                    lv_label_set_text(_directionLabel, currentText.c_str());  // Đặt lại text

                    // Buộc cập nhật và vẽ lại
                    lv_obj_invalidate(_directionLabel);
                }
            }
        }
        
        // Cập nhật dữ liệu mới
        _navData = navData;
        
        // Cập nhật hiển thị của default message label
        if (_defaultMessageLabel) {
            if (willBeActive) {
                // Nếu navigation active, ẩn thông báo mặc định
                lv_obj_add_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);
                Serial.println("updateNavigation: Hiding default message");
            } else {
                if(lv_obj_has_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN))
                {
                    lv_obj_clear_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);
                    // Reset vị trí cuộn text để bắt đầu chạy từ đầu
                    // Đặt lại text để bắt đầu hiệu ứng scroll từ đầu
                    const String currentText = String(lv_label_get_text(_defaultMessageLabel));
                    lv_label_set_text(_defaultMessageLabel, "");  // Xóa text
                    lv_label_set_text(_defaultMessageLabel, currentText.c_str());  // Đặt lại text

                    // Buộc cập nhật và vẽ lại
                    lv_obj_invalidate(_defaultMessageLabel);
                }
            }
        }
        
        // Cập nhật trạng thái icon - Cách tiếp cận đơn giản 
        _hasValidIcon = navData.hasIcon;
        // Nếu icon đã thay đổi, cập nhật dữ liệu - chỉ kiểm tra CRC khi có icon
        if (_hasValidIcon && _iconCRC != navData.iconCRC) {
            // Sao chép dữ liệu icon vào buffer thay vì chỉ lưu con trỏ
            memcpy(_iconBuffer, navData.icon, ICON_DATA_SIZE);
            _iconData = _iconBuffer;
            _iconCRC = navData.iconCRC;
            // Vẽ icon trực tiếp
            drawNavIconDirectly();
        } else if (!_hasValidIcon && _navIcon != nullptr) {
            lv_obj_del(_navIcon);
            _navIcon = nullptr;
            createContent();
        }

        // Cập nhật các nhãn văn bản
        updateLabels();
    }
    
    // Tạo màn hình điều hướng
    void create() {
        // Khởi tạo font tiếng Việt
        VietnameseFonts::init();
        
        // Xóa màn hình cũ nếu có
        if (_screen) {
            // Xóa tất cả các thành phần con trước khi xóa màn hình chính
            lv_obj_clean(_screen);
            
            // Sau đó xóa màn hình
            lv_obj_del(_screen);
            _screen = nullptr;
            
            // Đặt các con trỏ khác về NULL
            _navIcon = nullptr;
            _directionLabel = nullptr;
            _distanceLabel = nullptr;
            _titleLabel = nullptr;
            _timeLabel = nullptr;
            _speedLabel = nullptr;
            _durationLabel = nullptr;
            _bgImage = nullptr;
            _defaultMessageLabel = nullptr;
        }
        
        // Tạo màn hình mới
        _screen = lv_obj_create(NULL);
        
        // Kiểm tra xem màn hình đã được tạo thành công chưa
        if (!_screen) {
            Serial.println("ERROR: Failed to create navigation screen!");
            return;
        }
        
        // Tạo hình nền từ bg.c
        _bgImage = lv_img_create(_screen);
        lv_img_set_src(_bgImage, &bg_img);
        lv_obj_align(_bgImage, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_img_opa(_bgImage, 255, LV_PART_MAIN); // Hiển thị rõ nét 100%
        
        // Thiết lập màu nền cho screen
        lv_obj_set_style_bg_color(_screen, lv_color_hex(NavColors::BackgroundHex), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(_screen, 0, LV_PART_MAIN); // Trong suốt 100% để hiển thị hình nền rõ nét
        
        // Tạo các thành phần UI
        createHeader();
        createContent();
        createDefaultMessageLabel();

        updateUIBasedOnNavigationState();
        
        // Cập nhật nhãn với dữ liệu thực ngay lập tức
        updateLabels();
        
        Serial.println("NavigationScreenLVGL: Screen created successfully");
    }
    
    // Hiển thị màn hình
    void display() {
        if (_screen) {
            // Cập nhật dữ liệu trước khi hiển thị
            updateLabels();
            
            // Đảm bảo hình nền được hiển thị đúng và rõ nét
            if (_bgImage) {
                lv_obj_move_background(_bgImage); // Đảm bảo hình nền nằm dưới cùng
            }
            
            // Di chuyển default message lên trên hình nền nếu đang hiển thị
            if (_defaultMessageLabel && !lv_obj_has_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_move_foreground(_defaultMessageLabel);
            }
            
            // Xác định trạng thái active của navigation
            bool isActive = _navData.active && _navData.isNavigation;
            
            // Chỉ hiển thị các thành phần UI khi navigation active
            if (isActive) {
                // Di chuyển tất cả các phần tử UI lên trên hình nền
                if (_navIcon) lv_obj_move_foreground(_navIcon);
                if (_directionLabel && !lv_obj_has_flag(_directionLabel, LV_OBJ_FLAG_HIDDEN)) 
                    lv_obj_move_foreground(_directionLabel);
                if (_titleLabel) lv_obj_move_foreground(_titleLabel);
                if (_timeLabel) lv_obj_move_foreground(_timeLabel);
                if (_speedLabel && !lv_obj_has_flag(_speedLabel, LV_OBJ_FLAG_HIDDEN)) 
                    lv_obj_move_foreground(_speedLabel);
                if (_durationLabel) lv_obj_move_foreground(_durationLabel);
                
                // Đảm bảo container distance và label hiển thị trên cùng
                if (_distanceContainer && !lv_obj_has_flag(_distanceContainer, LV_OBJ_FLAG_HIDDEN)) {
                    // Loại bỏ thanh cuộn
                    lv_obj_clear_flag(_distanceContainer, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_clear_flag(_distanceLabel, LV_OBJ_FLAG_SCROLLABLE);
                    
                    // Đảm bảo kích thước container phù hợp
                    lv_obj_set_width(_distanceContainer, LV_SIZE_CONTENT);
                    lv_obj_update_layout(_distanceContainer);
                    lv_obj_center(_distanceLabel);
                    
                    // Đưa lên trên cùng
                    lv_obj_move_foreground(_distanceContainer);
                    lv_obj_move_foreground(_distanceLabel);
                    
                    Serial.printf("Display: Distance container size: w=%d, h=%d\n", 
                                lv_obj_get_width(_distanceContainer), 
                                lv_obj_get_height(_distanceContainer));
                }
            }
            
            // Chuyển sang màn hình navigation
            lv_scr_load(_screen);
            
            // Gọi cập nhật LVGL để vẽ ngay lập tức
            lv_refr_now(NULL);
        } else {
            Serial.println("ERROR: NavigationScreenLVGL - _screen is NULL, cannot display");
        }
    }
    
public:
    // Cập nhật giao diện dựa trên trạng thái active/inactive của navigation
    void updateUIBasedOnNavigationState() {
        bool isActive = _navData.active && _navData.isNavigation;

        if(isActive)
        {
            // Trước tiên ẩn default message
            lv_obj_add_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);

            // Hiện các thành phần UI
            lv_obj_clear_flag(_directionLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_speedLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_titleLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_timeLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_durationLabel, LV_OBJ_FLAG_HIDDEN);
            if(_navIcon)
            {
                lv_obj_clear_flag(_navIcon, LV_OBJ_FLAG_HIDDEN);
            }
            
            // Hiển thị container distance và cập nhật kích thước
            lv_obj_clear_flag(_distanceContainer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(_distanceLabel, LV_OBJ_FLAG_HIDDEN);
            
            // Force cập nhật kích thước container dựa trên nội dung
            lv_obj_set_width(_distanceContainer, LV_SIZE_CONTENT);
            lv_obj_update_layout(_distanceContainer);
            lv_obj_center(_distanceLabel);
            
            Serial.println("Navigation active: Showing all UI elements");
        }
        else
        {
            // Hiện default message
            if(lv_obj_has_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN))
            lv_obj_clear_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);
            {
                // Reset vị trí cuộn text để bắt đầu chạy từ đầu
                // Đặt lại text để bắt đầu hiệu ứng scroll từ đầu
                const String currentText = String(lv_label_get_text(_defaultMessageLabel));
                lv_label_set_text(_defaultMessageLabel, "");  // Xóa text
                lv_label_set_text(_defaultMessageLabel, currentText.c_str());  // Đặt lại text

                // Buộc cập nhật và vẽ lại
                lv_obj_invalidate(_defaultMessageLabel);
            }
            
            
            // Ẩn tất cả các thành phần UI khác
            lv_obj_add_flag(_directionLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_speedLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_distanceContainer, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_distanceLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_titleLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_timeLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(_durationLabel, LV_OBJ_FLAG_HIDDEN);
            if(_navIcon)
            {
                lv_obj_add_flag(_navIcon, LV_OBJ_FLAG_HIDDEN);
            }
            
            Serial.println("Navigation inactive: Only showing background and default message");
        }

        // Log trạng thái
        Serial.printf("Updated UI based on navigation state: isActive=%d\n", isActive);
    }

    private:

    // Tạo phần header (thanh trạng thái)
    void createHeader() {
        // Tạo nhãn time ở lề trái
        _timeLabel = VietnameseFonts::createText(
            _screen, 
            "", 
            VietnameseFonts::getNormalFont(), 
            lv_color_hex(0xFFFFFF),  // Màu trắng
            LV_ALIGN_TOP_LEFT,
            5, 5  // Vị trí lề trên bên trái
        );
        
        // Tạo nhãn duration ở lề phải
        _durationLabel = VietnameseFonts::createText(
            _screen, 
            "", 
            VietnameseFonts::getNormalFont(), 
            lv_color_hex(0xFFFFFF),  // Màu trắng
            LV_ALIGN_TOP_RIGHT,
            -5, 5  // Vị trí lề trên bên phải
        );
        
        // Thiết lập text color trực tiếp, không có nền
        lv_obj_set_style_text_color(_timeLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Màu trắng
        lv_obj_set_style_text_opa(_timeLabel, 255, LV_PART_MAIN); // Độ đậm tối đa
        lv_obj_set_style_text_color(_durationLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Màu trắng
        lv_obj_set_style_text_opa(_durationLabel, 255, LV_PART_MAIN); // Độ đậm tối đa
    }
    
    // Tạo phần nội dung (chỉ đường)
    void createContent() {
        // Tạo icon điều hướng ở lề trái
        if (_hasValidIcon && _iconData != nullptr) {
            // Vẽ icon trực tiếp sử dụng API bitmap của LVGL
            drawNavIconDirectly();
        }
        
        // Tạo nhãn title kế bên navIcon và cách mép 5px
        _titleLabel = VietnameseFonts::createText(
            _screen, 
            "", 
            VietnameseFonts::getSemiboldFont(), 
            lv_color_hex(0xFFFFFF),  // Màu trắng
            LV_ALIGN_TOP_LEFT,
            58, 40  // Vị trí ngay bên phải navIcon (48px + 5px)
        );
        
        // Tạo nhãn hướng dẫn (đơn giản)
        _directionLabel = lv_label_create(_screen);
        lv_obj_align(_directionLabel, LV_ALIGN_TOP_LEFT, 5, 85);
        lv_obj_set_width(_directionLabel, 230); // Giới hạn chiều rộng
        lv_obj_set_style_text_font(_directionLabel, VietnameseFonts::getBoldFont(), LV_PART_MAIN);
        lv_obj_set_style_text_color(_directionLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_text_opa(_directionLabel, 255, LV_PART_MAIN);
        
        // Thiết lập chế độ scroll và các thông số animation chỉ một lần
        lv_label_set_long_mode(_directionLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(_directionLabel, 40, LV_PART_MAIN);
        lv_obj_set_style_anim_time(_directionLabel, 5000, LV_PART_MAIN);
        
        lv_label_set_text(_directionLabel, "");
        
        // Tạo nhãn tốc độ
        _speedLabel = VietnameseFonts::createText(
            _screen,
            "",
            VietnameseFonts::getSemiboldFont(),
            lv_color_hex(0xFFFFFF),
            LV_ALIGN_BOTTOM_LEFT,
            5, -5  // Vị trí góc dưới bên trái
        );
        
        // Tạo container cho distance với nền màu vàng và bo tròn
        _distanceContainer = lv_obj_create(_screen);
        
        // Đặt kích thước theo nội dung thay vì cố định
        lv_obj_set_size(_distanceContainer, LV_SIZE_CONTENT, 35);
        lv_obj_align(_distanceContainer, LV_ALIGN_TOP_MID, 0, 130);

        // Thiết lập style cho container với nền màu vàng và bo tròn
        lv_obj_set_style_bg_color(_distanceContainer, lv_color_hex(0xFFDF00), LV_PART_MAIN); // Màu vàng
        lv_obj_set_style_bg_opa(_distanceContainer, 255, LV_PART_MAIN); // Độ đậm tối đa
        lv_obj_set_style_radius(_distanceContainer, 10, LV_PART_MAIN); // Bo tròn góc (giảm xuống)
        lv_obj_set_style_pad_all(_distanceContainer, 5, LV_PART_MAIN);  // Padding đồng đều
        lv_obj_set_style_pad_left(_distanceContainer, 15, LV_PART_MAIN); // Thêm padding bên trái
        lv_obj_set_style_pad_right(_distanceContainer, 15, LV_PART_MAIN); // Thêm padding bên phải
        lv_obj_set_style_border_width(_distanceContainer, 0, LV_PART_MAIN); // Không có viền
        
        // Loại bỏ thanh cuộn
        lv_obj_clear_flag(_distanceContainer, LV_OBJ_FLAG_SCROLLABLE);
        
        // Tạo nhãn khoảng cách bên trong container
        _distanceLabel = lv_label_create(_distanceContainer);
        lv_obj_set_size(_distanceLabel, LV_SIZE_CONTENT, LV_SIZE_CONTENT); // Tự động điều chỉnh kích thước
        lv_obj_set_style_text_font(_distanceLabel, VietnameseFonts::getBoldFont(), LV_PART_MAIN); // Font đậm
        lv_obj_set_style_text_color(_distanceLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Màu chữ trắng
        lv_obj_set_style_text_opa(_distanceLabel, 255, LV_PART_MAIN); // Độ đậm tối đa
        
        // Loại bỏ thanh cuộn cho label
        lv_obj_clear_flag(_distanceLabel, LV_OBJ_FLAG_SCROLLABLE);
        
        // Căn giữa trong container
        lv_obj_center(_distanceLabel);
        
        // Text mẫu để kiểm tra kích thước - đảm bảo container đủ rộng cho mọi text
        lv_label_set_text(_distanceLabel, "0 km");
        
        // Cập nhật layout để container có kích thước phù hợp với nội dung
        lv_obj_update_layout(_distanceContainer);
        
        // Log kích thước ban đầu
        Serial.printf("Initial distance container size: w=%d, h=%d\n", 
                    lv_obj_get_width(_distanceContainer), 
                    lv_obj_get_height(_distanceContainer));
    }
    
    // Tạo label hiển thị thông báo mặc định khi không có chỉ đường active
    void createDefaultMessageLabel() {
        // Tạo nhãn hiển thị "Start navigation on Google maps" ở giữa màn hình
        _defaultMessageLabel = lv_label_create(_screen);
        lv_obj_align(_defaultMessageLabel, LV_ALIGN_TOP_MID, 0, 75); // Căn giữa màn hình
        lv_obj_set_width(_defaultMessageLabel, 230); // Giới hạn chiều rộng
        lv_obj_set_style_text_font(_defaultMessageLabel, VietnameseFonts::getBoldFont(), LV_PART_MAIN);
        lv_obj_set_style_text_color(_defaultMessageLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // Màu trắng
        lv_obj_set_style_text_opa(_defaultMessageLabel, 255, LV_PART_MAIN);
        
        // Thiết lập chế độ scroll và các thông số animation
        lv_label_set_long_mode(_defaultMessageLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_speed(_defaultMessageLabel, 40, LV_PART_MAIN);
        lv_obj_set_style_anim_time(_defaultMessageLabel, 5000, LV_PART_MAIN);
        
        // Thiết lập text
        lv_label_set_text(_defaultMessageLabel, "Start navigation on Google maps");
        
        // Ban đầu, ẩn label này nếu navigation đang active
        if (_navData.active && _navData.isNavigation) {
            lv_obj_add_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(_defaultMessageLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Cập nhật nội dung các nhãn
    void updateLabels() {
        // Cập nhật hiển thị của các thành phần UI khác
        if (_directionLabel) {
            // Nếu navigation active, hiển thị nội dung hướng dẫn
            if (_navData.directions.length() > 0) {
                // Lưu text hiện tại
                const char* currentText = lv_label_get_text(_directionLabel);
                
                // Chỉ cập nhật nếu text thay đổi
                if (strcmp(currentText, _navData.directions.c_str()) != 0) {
                    // Đặt text mới
                    lv_label_set_text(_directionLabel, _navData.directions.c_str());
                    Serial.println("Updated direction text: " + _navData.directions);
                }
            } 
        }
        
        // Cập nhật title
        lv_label_set_text(_titleLabel, _navData.title.c_str());
        
        // Cập nhật thời gian hiện tại (góc trên bên trái)
        String currentTime;
        int hour = ChronosManager::getInstance().getChronos().getHour();
        int min = ChronosManager::getInstance().getChronos().getMinute();
        // Định dạng thời gian hiện tại "4:04" (không có AM/PM)
        currentTime = String(hour) + ":" + (min < 10 ? "0" : "") + String(min);
        lv_label_set_text(_timeLabel, currentTime.c_str());
        
        // Cập nhật khoảng cách trong container
        if (_navData.distance.length() > 0) {
            lv_label_set_text(_distanceLabel, _navData.distance.c_str());
        } else {
            lv_label_set_text(_distanceLabel, "0 km");
        }
        
        // Sau khi cập nhật nội dung, cần cập nhật layout của container để đảm bảo kích thước phù hợp
        if (_distanceContainer) {
            // Đảm bảo cập nhật kích thước container dựa trên nội dung
            lv_obj_update_layout(_distanceContainer);
            
            // Đảm bảo label được căn giữa trong container
            lv_obj_center(_distanceLabel);
        }
        
        // Cập nhật tốc độ (góc dưới bên trái)
        lv_label_set_text(_speedLabel, _navData.speed.c_str());
        // Cập nhật thời gian hành trình (góc trên bên phải)
        lv_label_set_text(_durationLabel, _navData.duration.c_str());
        
        // Đảm bảo các thành phần được cập nhật ngay lập tức
        if (_bgImage) {
            lv_obj_invalidate(_bgImage); // Đảm bảo background được vẽ lại đầu tiên
        }
        lv_obj_invalidate(_screen);
    }
};

#endif // NAVIGATION_SCREEN_LVGL_H
