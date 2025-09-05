#ifndef NAVIGATION_MANAGER_H
#define NAVIGATION_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "LGFX_Config.h"  // Đảm bảo LGFX được định nghĩa trước
#include "NavIconRenderer.h" // Thêm header cho NavIconRenderer
#include "BLEStatusOverlay.h"
#include "ChronosTypes.h"
#include "ChronosManager.h" // Di chuyển xuống sau LGFX_Config.h
#include "NavigationScreen.h" // Thêm header cho NavigationScreen mới

// ===== NAVIGATION MODE =====
enum class NavigationMode {
  NAV_DISABLED,   // Tắt chỉ đường
  BACKGROUND,     // Chạy nền (hiện thông báo nhỏ khi có rẽ)
  FULLSCREEN      // Chế độ chỉ đường toàn màn hình
};

// ===== QUẢN LÝ CHỈ ĐƯỜNG =====
class NavigationManager {
private:
  NavigationMode _navMode = NavigationMode::NAV_DISABLED;
  unsigned long _lastUpdateTime = 0;
  bool _needRedraw = false;
  bool _alertActive = false;
  unsigned long _alertStartTime = 0;
  static constexpr unsigned long ALERT_DURATION = 3000; // 3 giây
  LGFX* _tft = nullptr;
  
  // Dữ liệu chỉ đường đã lưu để so sánh thay đổi
  AppNavigation _lastNavData;
  
  // Thêm NavigationScreen instance
  NavigationScreen* _navScreen = nullptr;
  
public:
  // Cho phép truy cập Chronos từ bên ngoài (để khởi động lại quảng cáo)
  ChronosManager& getChronos() { return ChronosManager::getInstance(); }
  
public:
  NavigationManager() {
    // Initialize lastNavData with default values
    _lastNavData.active = false;
    _lastNavData.eta = "Navigation";
    _lastNavData.title = "Chronos";
    _lastNavData.duration = "Inactive";
    _lastNavData.distance = "";
    _lastNavData.speed = "";
    _lastNavData.directions = "Start navigation on Google maps";
    _lastNavData.hasIcon = false;
    _lastNavData.isNavigation = false;
    _lastNavData.iconCRC = 0xFFFFFFFF;
  }
  
  void init(LGFX* tft) {
    _tft = tft;
    
    // Khởi tạo NavigationScreen
    _navScreen = new NavigationScreen(*tft);
    
    // Khởi tạo Chronos Manager
    if (Config::NAVIGATION_ENABLED) {
      ChronosManager::getInstance().init(tft);
      
      // Cập nhật đối tượng Chronos cho NavigationScreen
      _navScreen->setChronos(&ChronosManager::getInstance().getChronos());
      
      Serial.println("Navigation Manager initialized");
      
      // Bắt đầu ở chế độ nền
      _navMode = NavigationMode::BACKGROUND;
    }
  }
  
  void update() {
    // Cập nhật Chronos Manager
    ChronosManager::getInstance().update();
    
    // Kiểm tra trạng thái kết nối BLE
    static bool lastConnectedState = false;
    bool currentConnectedState = ChronosManager::getInstance().isConnected();
    
    // Nếu trạng thái kết nối thay đổi
    if (lastConnectedState != currentConnectedState) {
      lastConnectedState = currentConnectedState;
      
      // Nếu mới kết nối, tự động chuyển sang chế độ FULLSCREEN
      if (currentConnectedState) {
        _navMode = NavigationMode::FULLSCREEN;
        _needRedraw = true;
        Serial.println("BLE connected - Switching to FULLSCREEN navigation mode");
        BLEStatusOverlay::getInstance().showConnected();
      } 
      // Nếu mới ngắt kết nối, tự động tắt chế độ điều hướng
      else {
        _navMode = NavigationMode::NAV_DISABLED;
        Serial.println("BLE disconnected - Navigation mode DISABLED");
        BLEStatusOverlay::getInstance().showDisconnected();
      }
    }
    
    // Nếu không kết nối hoặc đã tắt chế độ điều hướng, không xử lý tiếp
    if (_navMode == NavigationMode::NAV_DISABLED || !ChronosManager::getInstance().isConnected()) {
      return;
    }
    
    unsigned long currentTime = millis();
    
    // Kiểm tra thông tin chỉ đường mới
    if (currentTime - _lastUpdateTime >= Config::NAV_UPDATE_INTERVAL) {
      _lastUpdateTime = currentTime;
      
      if (ChronosManager::getInstance().isNavigating()) {
        AppNavigation navData = ChronosManager::getInstance().getNavData();
        
        // Kiểm tra nếu có thay đổi từ dữ liệu trước đó
        bool dataChanged = (_lastNavData.active != navData.active) || 
                          (_lastNavData.title != navData.title) ||
                          (_lastNavData.directions != navData.directions) ||
                          (_lastNavData.distance != navData.distance);
        
        // Kiểm tra nếu cần hiển thị cảnh báo rẽ
        float distance = navData.distance.toFloat();
        if (distance <= Config::NAV_ALERT_DISTANCE && !_alertActive && navData.active) {
          _alertActive = true;
          _alertStartTime = currentTime;
          _needRedraw = true;
          Serial.printf("Navigation alert! Distance: %.1fm, Direction: %s\n", 
                      distance, navData.directions.c_str());
        }
        
        // Cập nhật trạng thái cảnh báo
        if (_alertActive && currentTime - _alertStartTime >= ALERT_DURATION) {
          _alertActive = false;
        }
        
        // Cập nhật màn hình nếu có thay đổi hoặc cảnh báo
        if (dataChanged || _needRedraw) {
          if (_navMode == NavigationMode::FULLSCREEN) {
            drawFullscreenNavigation(navData);
          }
          
          // Lưu dữ liệu mới nhất
          _lastNavData = navData;
          _needRedraw = false;
        }
      }
    }
  }
  
  // Chuyển đổi giữa các chế độ chỉ đường
  void toggleNavigationMode() {
    if (!ChronosManager::getInstance().isConnected()) {
      _navMode = NavigationMode::NAV_DISABLED;
      return;
    }
    
    switch (_navMode) {
      case NavigationMode::BACKGROUND:
        _navMode = NavigationMode::FULLSCREEN;
        break;
      case NavigationMode::FULLSCREEN:
        _navMode = NavigationMode::BACKGROUND;
        break;
      case NavigationMode::NAV_DISABLED:
        _navMode = NavigationMode::FULLSCREEN;
        break;
    }
    
    _needRedraw = true;
    Serial.printf("Navigation mode toggled to: %d\n", (int)_navMode);
  }
  
  bool isConnected() {
    return ChronosManager::getInstance().isConnected();
  }
  
  bool isNavigating() {
    return ChronosManager::getInstance().isNavigating();
  }
  
  NavigationMode getNavigationMode() {
    return _navMode;
  }
  
  // Lấy dữ liệu điều hướng hiện tại
  AppNavigation getNavData() {
    return ChronosManager::getInstance().getNavData();
  }
  
  // Hiển thị điều hướng toàn màn hình sử dụng NavigationScreen
  void drawFullscreenNavigation(const AppNavigation& navData) {
    if (!_tft || !_navScreen) return;
    
    // Chuyển đổi từ AppNavigation sang Navigation để sử dụng với NavigationScreen
    const Navigation& chronosNav = ChronosManager::getInstance().getChronos().getNavigation();
    
    // Cập nhật đối tượng Chronos cho NavigationScreen
    _navScreen->setChronos(&ChronosManager::getInstance().getChronos());
    
    // Cập nhật dữ liệu điều hướng cho NavigationScreen
    _navScreen->updateNavigation(chronosNav);
    
    // Hiển thị màn hình điều hướng mới theo thiết kế smartwatch Android
    _navScreen->display();
    
    // In thông tin debug
    Serial.println("Displaying new navigation screen (Android style)");
    Serial.println("Title: [" + chronosNav.title + "]");
    Serial.println("Directions: [" + chronosNav.directions + "]");
    Serial.println("Distance: [" + chronosNav.distance + "]");
    Serial.println("Duration: [" + chronosNav.duration + "]");
    Serial.println("ETA: [" + chronosNav.eta + "]");
    
    char timeStr[10];
    sprintf(timeStr, "%02d:%02d", 
            ChronosManager::getInstance().getChronos().getHour(), 
            ChronosManager::getInstance().getChronos().getMinute());
    Serial.printf("Current Time: %s\n", timeStr);
  }
  
  static NavigationManager& getInstance() {
    static NavigationManager instance;
    return instance;
  }
};

#endif // NAVIGATION_MANAGER_H
