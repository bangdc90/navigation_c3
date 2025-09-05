#ifndef BLE_STATUS_OVERLAY_H
#define BLE_STATUS_OVERLAY_H

#include <Arduino.h>
#include "LGFX_Config.h"

// Lớp hiển thị trạng thái kết nối BLE
class BLEStatusOverlay {
private:
  LGFX* _tft = nullptr;
  bool _isShowing = false;
  unsigned long _showStartTime = 0;
  static constexpr unsigned long SHOW_DURATION = 2000; // 2 giây
  
public:
  BLEStatusOverlay() {}
  
  void init(LGFX* tft) {
    _tft = tft;
  }
  
  // Hiển thị thông báo kết nối BLE
  void showConnected() {
    if (!_tft) return;
    
    // Hiển thị overlay thông báo ở góc phải trên
    int x = _tft->width() - 100;
    int y = 5;
    int width = 95;
    int height = 30;
    
    _tft->fillRoundRect(x, y, width, height, 5, TFT_DARKGREEN);
    _tft->drawRoundRect(x, y, width, height, 5, TFT_GREEN);
    
    _tft->setTextColor(TFT_WHITE);
    _tft->setTextSize(1);
    _tft->setCursor(x + 10, y + 10);
    _tft->println("BLE KET NOI");
    
    _isShowing = true;
    _showStartTime = millis();
  }
  
  // Hiển thị thông báo ngắt kết nối BLE
  void showDisconnected() {
    if (!_tft) return;
    
    // Hiển thị overlay thông báo ở góc phải trên
    int x = _tft->width() - 100;
    int y = 5;
    int width = 95;
    int height = 30;
    
    _tft->fillRoundRect(x, y, width, height, 5, TFT_MAROON);
    _tft->drawRoundRect(x, y, width, height, 5, TFT_RED);
    
    _tft->setTextColor(TFT_WHITE);
    _tft->setTextSize(1);
    _tft->setCursor(x + 10, y + 10);
    _tft->println("BLE NGAT KET NOI");
    
    _isShowing = true;
    _showStartTime = millis();
  }
  
  // Cập nhật và kiểm tra nếu cần tắt thông báo
  void update() {
    if (_isShowing && millis() - _showStartTime >= SHOW_DURATION) {
      _isShowing = false;
      // Không cần xóa thông báo vì màn hình sẽ được cập nhật bởi video hoặc điều hướng
    }
  }
  
  bool isShowing() {
    return _isShowing;
  }
  
  static BLEStatusOverlay& getInstance() {
    static BLEStatusOverlay instance;
    return instance;
  }
};

#endif // BLE_STATUS_OVERLAY_H
