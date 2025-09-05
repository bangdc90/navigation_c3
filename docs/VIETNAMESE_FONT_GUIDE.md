## Hướng dẫn hiển thị tiếng Việt trên màn hình ST7789 với ESP32-C3

### Giới thiệu

Tài liệu này hướng dẫn cách hiển thị tiếng Việt đầy đủ dấu trên màn hình ST7789 sử dụng ESP32-C3 với thư viện LovyanGFX.

### Các phương pháp hiển thị tiếng Việt

1. **Sử dụng font tiếng Việt tùy chỉnh**
   - Chuyển đổi font TTF tiếng Việt sang định dạng font bitmap
   - Sử dụng công cụ chuyển đổi như U8g2 Font Converter, GLCD Font Creator hoặc Adafruit GFX Font Converter

2. **Sử dụng ký tự thay thế ASCII**
   - Phương pháp này sử dụng các ký tự ASCII để thay thế các ký tự tiếng Việt có dấu
   - Ví dụ: "á" → "a'", "ê" → "e^", "đ" → "d-"

3. **Tích hợp với U8g2**
   - Sử dụng thư viện U8g2 kết hợp với LovyanGFX
   - U8g2 hỗ trợ nhiều font Unicode bao gồm cả tiếng Việt

4. **Sử dụng LVGL (Light and Versatile Graphics Library)**
   - LVGL là thư viện đồ họa mạnh mẽ hỗ trợ đầy đủ Unicode
   - Có thể tích hợp LVGL với LovyanGFX

### Cài đặt

1. **Thêm thư viện LovyanGFX** 
   ```ini
   lib_deps = 
       lovyan03/LovyanGFX@1.2.7
   ```

2. **Cài đặt U8g2 (tùy chọn)**
   ```ini
   lib_deps = 
       lovyan03/LovyanGFX@1.2.7
       olikraus/U8g2@^2.34.22
   ```

3. **Cài đặt LVGL (tùy chọn)**
   ```ini
   lib_deps = 
       lovyan03/LovyanGFX@1.2.7
       lvgl/lvgl@^8.3.9
   ```

### Các file trong dự án

1. **VietnameseFonts.h**
   - Định nghĩa font và phương thức xử lý tiếng Việt
   - Bảng chuyển đổi ký tự UTF-8 sang dạng dễ hiển thị

2. **GFXFontVietnamese.h**
   - Lớp xử lý font tiếng Việt cho LovyanGFX
   - Cung cấp các phương thức vẽ chuỗi UTF-8

3. **U8g2_LovyanGFX.h**
   - Lớp kết nối U8g2 với LovyanGFX
   - Cho phép sử dụng các font Unicode của U8g2

### Sử dụng phương pháp ký tự thay thế ASCII

```cpp
// Chuyển đổi chuỗi tiếng Việt sang định dạng hiển thị
String displayText = convertToDisplayText("Tiếng Việt có dấu");
lcd.print(displayText); // Kết quả: "Tie^'ng Vie^.t co' da^'u"
```

### Sử dụng font Unicode với U8g2

```cpp
#include "U8g2_LovyanGFX.h"

U8g2_for_LovyanGFX u8g2;
LGFX lcd;

void setup() {
  lcd.init();
  u8g2.begin(lcd);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese);
  u8g2.drawUTF8(10, 20, "Tiếng Việt có dấu");
}
```

### Ví dụ

Xem file ví dụ `examples/VietnameseFontExample/VietnameseFontExample.ino` để biết cách sử dụng các phương pháp hiển thị tiếng Việt.

### Nguồn tham khảo

- [LovyanGFX](https://github.com/lovyan03/LovyanGFX)
- [U8g2](https://github.com/olikraus/U8g2)
- [Adafruit GFX Font Converter](https://rop.nl/truetype2gfx/)
- [LVGL](https://lvgl.io/)
- [GLCD Font Creator](https://www.mikroe.com/glcd-font-creator)
