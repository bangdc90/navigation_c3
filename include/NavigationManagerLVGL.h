#ifndef NAVIGATION_MANAGER_LVGL_H
#define NAVIGATION_MANAGER_LVGL_H

#include <Arduino.h>
#include "Config.h"
#include "LVGL_Config.h"
#include "NavigationScreenLVGL.h"
#include "BLEStatusOverlay.h"
#include "ChronosTypes.h"
#include "ChronosManager.h" // Thêm ChronosManager để quản lý kết nối BLE
#include "ESP32Time.h"

// ===== NAVIGATION MODE =====
enum class NavigationMode {
  NAV_DISABLED,   // Tắt chỉ đường
  BACKGROUND,     // Chạy nền (hiện thông báo nhỏ khi có rẽ)
  FULLSCREEN      // Chế độ chỉ đường toàn màn hình
};

// ===== QUẢN LÝ CHỈ ĐƯỜNG =====
class NavigationManagerLVGL {
private:
  NavigationMode _navMode = NavigationMode::NAV_DISABLED;
  unsigned long _lastUpdateTime = 0;
  bool _needRedraw = false;
  bool _alertActive = false;
  unsigned long _alertStartTime = 0;
  static constexpr unsigned long ALERT_DURATION = 3000; // 3 giây
  LVGL_Display* _display = nullptr;
  ESP32Time* _time = nullptr;
  
  // Dữ liệu chỉ đường đã lưu để so sánh thay đổi
  AppNavigation _navData;
  
  // Đối tượng NavigationScreenLVGL
  NavigationScreenLVGL* _navScreen = nullptr;
  
public:
  // Cho phép truy cập Chronos từ bên ngoài (để khởi động lại quảng cáo)
  ChronosManager& getChronos() { return ChronosManager::getInstance(); }

public:
  NavigationManagerLVGL() {
    // Initialize navigation data with default values
    _navData.active = false;
    _navData.eta = "";
    _navData.title = "";
    _navData.duration = "";
    _navData.distance = "";
    _navData.speed = "";
    _navData.directions = "";
    _navData.hasIcon = false;
    _navData.isNavigation = false;
    _navData.iconCRC = 0xFFFFFFFF;
    
    // Khởi tạo ESP32Time
    _time = new ESP32Time();
  }
  
  ~NavigationManagerLVGL() {
    if (_navScreen) {
      delete _navScreen;
      _navScreen = nullptr;
    }
    
    if (_time) {
      delete _time;
      _time = nullptr;
    }
  }
  
  void init(LVGL_Display* display) {
    _display = display;
    
    // Khởi tạo NavigationScreen LVGL
    _navScreen = new NavigationScreenLVGL();
    _navScreen->create();
    
    // Khởi tạo Chronos Manager
    if (Config::NAVIGATION_ENABLED) {
      ChronosManager::getInstance().init(display->getTft());
      
      Serial.println("Navigation Manager initialized with LVGL");
      
      // Bắt đầu ở chế độ nền
      _navMode = NavigationMode::BACKGROUND;
    }
  }
  
  void update() {
    // Cập nhật LVGL display
    if (_display) {
      _display->update();
    }
    
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
      } 
      // Nếu mới ngắt kết nối, tự động tắt chế độ điều hướng
      else {
        _navMode = NavigationMode::NAV_DISABLED;
        _needRedraw = false; // Không cần vẽ lại màn hình navigation
        Serial.println("BLE disconnected - Navigation mode DISABLED");
        
        // Khi ngắt kết nối, đảm bảo màn hình navigation không hiển thị
        if (_navScreen && _navScreen->isScreenReady()) {
          // Không gọi display() ở đây để tránh hiển thị màn hình navigation
          _navScreen->create(); // Tạo lại screen (sẽ ẩn các thành phần)
        }
        
        // Ngay lập tức thoát khỏi hàm update để không cập nhật màn hình navigation nữa
        return;
      }
    }
    
    // Nếu không kết nối hoặc đã tắt chế độ điều hướng, không xử lý tiếp
    if (_navMode == NavigationMode::NAV_DISABLED || !ChronosManager::getInstance().isConnected()) {
      return;
    }
    
    // Cập nhật thời gian
    unsigned long currentTime = millis();
    
    // Nếu đang ở chế độ FULLSCREEN, đảm bảo màn hình luôn được hiển thị
    static unsigned long lastRefreshTime = 0;
    if (_navMode == NavigationMode::FULLSCREEN && currentTime - lastRefreshTime >= 500) {
      lastRefreshTime = currentTime;
      
      // Đảm bảo screen luôn được hiển thị mỗi 500ms kể cả không có dữ liệu mới
      if (_navScreen && _navScreen->isScreenReady()) {
        _navScreen->display();
      }
    }
    
    // Kiểm tra thông tin chỉ đường mới (chỉ mỗi 1 giây)
    if (currentTime - _lastUpdateTime >= Config::NAV_UPDATE_INTERVAL) {
      _lastUpdateTime = currentTime;
      
      // Lấy dữ liệu navigation hiện tại
      AppNavigation navData = ChronosManager::getInstance().getNavData();
      
      // Kiểm tra nếu navigation chuyển từ active sang inactive
      bool wasActive = _navData.active && _navData.isNavigation;
      bool isActive = navData.active && navData.isNavigation;

      // Nếu chuyển từ active -> inactive, cần thiết lập lại UI
      if (wasActive && !isActive) {
        Serial.println("Navigation changed from active to inactive, updating UI");
        _needRedraw = true;
        // Cập nhật dữ liệu và vẽ lại màn hình
        if (_navMode == NavigationMode::FULLSCREEN && _navScreen && _navScreen->isScreenReady()) {
          // Đảm bảo UI được cập nhật đúng
          _navScreen->updateNavigation(navData);
         
          drawFullscreenNavigation(navData);
        }
      }
      
      if (ChronosManager::getInstance().isNavigating()) {
        // Kiểm tra nếu có thay đổi từ dữ liệu trước đó
        bool dataChanged = (_navData.active != navData.active) || 
                          (_navData.title != navData.title) ||
                          (_navData.directions != navData.directions) ||
                          (_navData.distance != navData.distance);
        
        // Kiểm tra nếu cần hiển thị cảnh báo rẽ
        float distance = 0;
        bool validDistance = false;
        
        // Kiểm tra định dạng distance hợp lệ
        if (navData.distance.length() > 0) {
          char firstChar = navData.distance.charAt(0);
          if (isDigit(firstChar) || firstChar == '.') {
            distance = navData.distance.toFloat();
            validDistance = true;
          }
        }
        
        if (validDistance && distance <= Config::NAV_ALERT_DISTANCE && !_alertActive && navData.active) {
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
        
        // Chỉ cập nhật màn hình khi có thay đổi dữ liệu hoặc cần cảnh báo
        if (dataChanged || _needRedraw) {
          if (_navMode == NavigationMode::FULLSCREEN) {
            drawFullscreenNavigation(navData);
          }
          
          // Lưu dữ liệu mới nhất
          _navData = navData;
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
  
  // Cập nhật dữ liệu điều hướng
  void updateNavData(const AppNavigation& navData) {
    // Kiểm tra nếu navigation chuyển từ active sang inactive
    bool wasActive = _navData.active && _navData.isNavigation;
    bool isActive = navData.active && navData.isNavigation;
    
    // Nếu thay đổi trạng thái active hoặc dữ liệu đang active, cần vẽ lại
    if (wasActive != isActive || isActive) {
      _navData = navData;
      _needRedraw = true;
      
      // Ghi log khi trạng thái navigation thay đổi
      if (wasActive && !isActive) {
        Serial.println("Navigation changed from active to inactive (updateNavData)");
      } else if (!wasActive && isActive) {
        Serial.println("Navigation changed from inactive to active (updateNavData)");
      }
      
      // Vẽ lại màn hình ngay lập tức nếu đang ở chế độ FULLSCREEN
      if (_navMode == NavigationMode::FULLSCREEN && _navScreen && _navScreen->isScreenReady()) {
        drawFullscreenNavigation(navData);
      }
    }
  }
  
  // Hiển thị điều hướng toàn màn hình sử dụng NavigationScreenLVGL
  void drawFullscreenNavigation(const AppNavigation& navData) {
    if (!_navScreen) {
      Serial.println("Cannot display navigation: _navScreen is null");
      return;
    }
    
    // Cập nhật dữ liệu điều hướng cho NavigationScreen
    _navScreen->updateNavigation(navData);
    
    // Xác định trạng thái active của navigation
    bool isActive = navData.active && navData.isNavigation;
    
    // Xác minh screen đã được tạo
    if (_navScreen->isScreenReady()) {
      // Chỉ hiển thị khi kết nối và không ở chế độ NAV_DISABLED
      if (ChronosManager::getInstance().isConnected() && _navMode != NavigationMode::NAV_DISABLED) {
        _navScreen->display();
        
        _navScreen->updateUIBasedOnNavigationState();
        
        // Đảm bảo LVGL được cập nhật ngay lập tức
        LVGL_Display::getInstance().update();
      }
    } else {
      Serial.println("ERROR: Navigation screen not ready, recreating...");
      // Thử tạo lại screen
      _navScreen->create();
      if (_navScreen->isScreenReady()) {
        // Chỉ hiển thị khi kết nối và không ở chế độ NAV_DISABLED
        if (ChronosManager::getInstance().isConnected() && _navMode != NavigationMode::NAV_DISABLED) {
          _navScreen->display();
          LVGL_Display::getInstance().update();
        }
      }
    }
    
    // Lưu dữ liệu mới nhất
    _navData = navData;
  }
  
  static NavigationManagerLVGL& getInstance() {
    static NavigationManagerLVGL instance;
    return instance;
  }
};

#endif // NAVIGATION_MANAGER_LVGL_H
