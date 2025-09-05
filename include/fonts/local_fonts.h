#ifndef LOCAL_FONTS_H
#define LOCAL_FONTS_H

// Đảm bảo LVGL_CONF_INCLUDE_SIMPLE được định nghĩa trước khi include lvgl.h
#include "../../.pio/libdeps/esp32-c3-devkitm-1/lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif
lv_font_t* get_montserrat_24();
lv_font_t* get_montserrat_bold_32();
lv_font_t* get_montserrat_number_bold_48();
lv_font_t* get_montserrat_semibold_24();
lv_font_t* get_montserrat_semibold_28();
#ifdef __cplusplus
}
#endif

#endif // LOCAL_FONTS_H