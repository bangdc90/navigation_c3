#ifndef CHRONOS_MANAGER_H
#define CHRONOS_MANAGER_H

#include <Arduino.h>
#include "Config.h"
// Include our types first
#include "ChronosTypes.h"
// Then include ChronosESP32 adapter
#include "ChronosESP32Adapter.h"

// Forward declarations
class LGFX;
class Navigation;

// Lớp quản lý tương tác với Chronos app sử dụng thư viện ChronosESP32
class ChronosManager {
private:
    ChronosESP32Adapter* chronos = nullptr;
    LGFX* _tft = nullptr;
    bool _isConnected = false;
    bool _isNavigating = false;
    String _address;
    
    // Dữ liệu điều hướng hiện tại
    AppNavigation _navData;
    
    // Callback khi nhận được biểu tượng điều hướng
    static void iconCallbackHandler(uint8_t icon, String data) {
        ChronosManager& instance = getInstance();
        
        Serial.println("Received navigation icon: " + String(icon));
        instance._navData.hasIcon = true;
        instance.handleConfigChange(ConfigType::CF_NAV_ICON, icon, 0);
    }
    
    // Callback khi trạng thái kết nối thay đổi
    static void connectCallbackHandler(bool connected) {
        ChronosManager& instance = getInstance();
        instance._isConnected = connected;
        
        Serial.println(connected ? "BLE connected to Chronos app" : "BLE disconnected from Chronos app");
        
        // Gọi hàm xử lý thay đổi kết nối
        instance.handleConnectionChange(connected);
    }
    
    // Callback khi nhận được thông tin config - thiết kế giống hệt với ví dụ
    static void configCallbackHandler(ConfigType config, uint32_t value1, uint32_t value2) {
        ChronosManager& instance = getInstance();
        
        switch (config)
        {
        case ConfigType::CF_NAV_DATA:
            Serial.print("Navigation state: ");
            Serial.println(value1 ? "Active" : "Inactive");
            
            // Đánh dấu trạng thái navigation đã thay đổi
            instance._isNavigating = value1 == 1;
            instance._navData.active = value1 == 1;
            
            // Nếu navigation đang active, lấy dữ liệu navigation
            if (value1) {
                if (instance.chronos != nullptr) {
                    // Lấy thông tin navigation từ đối tượng ChronosESP32
                    Navigation nav = instance.chronos->getNavigation();
                    
                    // Cập nhật dữ liệu navigation theo thứ tự mới
                    instance._navData.distance = nav.distance;
                    instance._navData.duration = nav.duration;
                    instance._navData.eta = nav.eta;
                    instance._navData.directions = nav.directions;
                    instance._navData.title = nav.title;
                    instance._navData.speed = nav.speed;
                    instance._navData.hasIcon = nav.hasIcon;
                    instance._navData.isNavigation = nav.isNavigation;
                    
                    // Sao chép dữ liệu icon nếu có
                    if (nav.hasIcon) {
                        memcpy(instance._navData.icon, nav.icon, ICON_DATA_SIZE);
                        instance._navData.iconCRC = nav.iconCRC;
                    }
                    
                    // In thông tin debug theo thứ tự mới
                    Serial.println(nav.distance);
                    Serial.println(nav.duration);
                    Serial.println(nav.eta);
                    Serial.println(nav.directions);
                    Serial.println(nav.title);
                    Serial.println(nav.speed);
                }
            }
            break;
            
        case ConfigType::CF_NAV_ICON:
            Serial.print("CF_NAV_ICON Navigation Icon data, position: ");
            Serial.println(value1);
            Serial.print("Icon CRC: ");
            Serial.printf("0x%08X\n", value2);
            
            // Xử lý khi nhận được dữ liệu icon
            if (instance.chronos != nullptr) {
                Navigation nav = instance.chronos->getNavigation();
                instance._navData.hasIcon = nav.hasIcon;
                instance._navData.iconCRC = nav.iconCRC;
                
                // Sao chép dữ liệu icon từ Navigation sang AppNavigation
                if (nav.hasIcon) {
                    // Sao chép toàn bộ dữ liệu icon
                    memcpy(instance._navData.icon, nav.icon, ICON_DATA_SIZE);
                }
            }
            break;
            
        case ConfigType::CF_TIME:
            Serial.println("Received time sync from app");
            break;
            
        default:
            Serial.printf("Received config type: %d, values: %u, %u\n", 
                         (int)config, value1, value2);
            break;
        }
    }
    
    // Callback khi nhận được thông báo
    static void notificationCallbackHandler(Notification notification) {
        Serial.print("Notification received at ");
        Serial.println(notification.time);
        Serial.print("From: ");
        Serial.print(notification.app);
        Serial.print("\tIcon: ");
        Serial.println(notification.icon);
        Serial.println(notification.title);
        Serial.println(notification.message);
    }
    
    // Callback khi nhận được thông tin thời gian
    static void timeCallbackHandler(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year) {
        ChronosManager& instance = getInstance();
        
        Serial.printf("Received time: %02d:%02d:%02d %02d/%02d/%04d\n", 
                     hour, minute, second, day, month, year);
        
        instance.handleConfigChange(ConfigType::CF_TIME, 0, 0);
    }

public:
    // Lấy đối tượng Chronos gốc
    ChronosESP32Patched& getChronos() {
        return chronos->getChronos();
    }
    ChronosManager() {
        // Initialize navigation data with default values
        _navData.active = false;
        _navData.eta = "Navigation";
        _navData.title = "MrVocSi";
        _navData.duration = "";
        _navData.distance = "";
        _navData.speed = "";
        _navData.directions = "";
        _navData.hasIcon = false;
        _navData.isNavigation = false;
        _navData.iconCRC = 0xFFFFFFFF;
        
        // Tạo đối tượng adapter cho ChronosESP32
        chronos = new ChronosESP32Adapter();
    }
    
    ~ChronosManager() {
        if (chronos != nullptr) {
            delete chronos;
            chronos = nullptr;
        }
    }
    
    void init(LGFX* tft) {
        _tft = tft;
        
        Serial.println("Initializing Chronos Manager with ChronosESP32 library...");
        
        // Đăng ký tất cả các callback trước khi khởi tạo, giống như trong ví dụ
        if (chronos != nullptr) {
            // Đăng ký các callbacks giống hệt với file ví dụ
            chronos->setConnectionCallback(connectCallbackHandler);
            chronos->setNotificationCallback(notificationCallbackHandler);
            chronos->setConfigurationCallback(configCallbackHandler);
            
            Serial.println("All callbacks registered successfully");
        }
        
        // Khởi tạo BLE với tên thiết bị chính xác "Chronos"
        if (chronos->begin(false)) {
            Serial.println("ChronosESP32 initialized successfully");
            
            // Lưu địa chỉ BLE
            _address = NimBLEDevice::getAddress().toString().c_str();
            
            // Thiết lập mức pin
            chronos->setBattery(100, false);
            
            Serial.println("Chronos Manager initialized");
            Serial.println("BLE address: " + _address);
        } else {
            Serial.println("Failed to initialize ChronosESP32");
        }
    }
    
    void update() {
        // Gọi loop() của ChronosESP32 để xử lý các tác vụ nội bộ, như trong ví dụ
        if (chronos != nullptr) {
            chronos->loop();
        }
        
        // Kiểm tra trạng thái kết nối BLE
        bool currentConnected = isConnected();
        static bool lastConnected = false;
        
        // Xử lý thay đổi trạng thái kết nối
        if (currentConnected != lastConnected) {
            if (currentConnected) {
                // Mới kết nối
                handleConnectionChange(true);
                Serial.println("ChronosManager: BLE connected");
            } else {
                // Mới ngắt kết nối
                handleConnectionChange(false);
                Serial.println("ChronosManager: BLE disconnected");
            }
            lastConnected = currentConnected;
        }
        
        // In thông tin điều hướng hiện tại ra Serial mỗi 10 giây
        static unsigned long lastNavLog = 0;
        if (_isNavigating && millis() - lastNavLog > 10000) {
            lastNavLog = millis();
            Serial.println("Navigation state: " + String(_isNavigating ? "ACTIVE" : "INACTIVE"));
            
            if (_isNavigating) {
                Serial.println("\n===== CURRENT NAVIGATION DATA =====");
                Serial.println("- Active: " + String(_navData.active ? "true" : "false"));
                Serial.println("- HasIcon: " + String(_navData.hasIcon ? "true" : "false"));
                Serial.println("- IconCRC: 0x" + String(_navData.iconCRC, HEX));
                Serial.println("- Title: " + _navData.title);
                Serial.println("- Distance: " + _navData.distance);
                Serial.println("- Duration: " + _navData.duration);
                Serial.println("- ETA: " + _navData.eta);
                Serial.println("- Directions: " + _navData.directions);
                Serial.println("=================================");
            }
        }
    }
    
    bool isConnected() {
        if (chronos != nullptr) {
            return chronos->isConnected();
        }
        return _isConnected;
    }
    
    bool isNavigating() {
        return _isNavigating;
    }
    
    AppNavigation getNavData() {
        return _navData;
    }
    
    // Gửi lệnh tùy chỉnh đến thiết bị Chronos
    void sendCommand(uint8_t* command, size_t length) {
        if (chronos != nullptr) {
            Serial.println("Sending custom command to Chronos device");
            for (size_t i = 0; i < length; i++) {
                Serial.printf("0x%02X ", command[i]);
            }
            Serial.println();
            
            // Sử dụng adapter để gửi lệnh
            chronos->sendCommand(command, length);
        } else {
            Serial.println("Failed to send command: Chronos adapter not initialized");
        }
    }
    
    // Xử lý khi trạng thái kết nối thay đổi
    void handleConnectionChange(bool connected) {
        if (connected) {
            Serial.println("BLE connected to Chronos app");
        } else {
            Serial.println("BLE disconnected from Chronos app");
            _isNavigating = false;
            _navData.active = false;
        }
    }
    
    // Chuyển đổi từ ConfigType sang AppConfigType để sử dụng trong nội bộ
    AppConfigType convertConfigType(ConfigType config) {
        switch(config) {
            case ConfigType::CF_TIME:
                return AppConfigType::CF_TIME;
            case ConfigType::CF_NAV_DATA:
                return AppConfigType::CF_NAV_DATA;
            case ConfigType::CF_NAV_ICON:
                return AppConfigType::CF_NAV_ICON;
            case ConfigType::CF_PBAT:
                return AppConfigType::CF_PBAT;
            case ConfigType::CF_ALARM:
                return AppConfigType::CF_ALARM;
            case ConfigType::CF_USER:
                return AppConfigType::CF_USER;
            case ConfigType::CF_SED:
                return AppConfigType::CF_SED;
            case ConfigType::CF_QUIET:
                return AppConfigType::CF_QUIET;
            case ConfigType::CF_RTW:
                return AppConfigType::CF_RTW;
            case ConfigType::CF_HOURLY:
                return AppConfigType::CF_HOURLY;
            case ConfigType::CF_CAMERA:
                return AppConfigType::CF_CAMERA;
            case ConfigType::CF_LANG:
                return AppConfigType::CF_LANG;
            case ConfigType::CF_HR24:
                return AppConfigType::CF_HR24;
            case ConfigType::CF_SLEEP:
                return AppConfigType::CF_SLEEP;
            case ConfigType::CF_APP:
                return AppConfigType::CF_APP;
            case ConfigType::CF_CONTACT:
                return AppConfigType::CF_CONTACT;
            case ConfigType::CF_QR:
                return AppConfigType::CF_QR;
            case ConfigType::CF_FONT:
                return AppConfigType::CF_FONT;
            case ConfigType::CF_RST:
                return AppConfigType::CF_RST;
            default:
                // Trường hợp mặc định, trả về CF_TIME
                return AppConfigType::CF_TIME;
        }
    }
    
    // Xử lý khi nhận được cấu hình từ app
    void handleConfigChange(ConfigType config, uint32_t value1, uint32_t value2) {
        // Chuyển đổi sang AppConfigType
        AppConfigType appConfig = convertConfigType(config);
        
        switch(appConfig) {
            case AppConfigType::CF_TIME:
                Serial.println("Received time sync from app");
                break;
                
            case AppConfigType::CF_NAV_DATA:
                _isNavigating = value1 == 1;
                Serial.printf("Navigation status changed: %s\n", _isNavigating ? "Active" : "Inactive");
                break;
                
            case AppConfigType::CF_NAV_ICON:
                Serial.printf("Received navigation icon: %d\n", (int)value1);
                break;
                
            default:
                break;
        }
    }
    
    // Khởi động lại quảng cáo BLE để khôi phục kết nối
    void restartAdvertising() {
        if (chronos != nullptr) {
            Serial.println("Restarting BLE advertising");
            chronos->restart();
        }
    }
    
    // Hàm debug hiển thị trạng thái hiện tại
    void debugCurrentState() {
        Serial.println("\n============ CHRONOS BLE STATUS ============");
        Serial.printf("Connected: %s\n", isConnected() ? "YES" : "NO");
        Serial.printf("Navigation Active: %s\n", _isNavigating ? "YES" : "NO");
        Serial.printf("BLE Address: %s\n", _address.c_str());
        
        // Nếu kết nối, thử lấy dữ liệu navigation từ thư viện
        if (chronos != nullptr && chronos->isConnected()) {
            Navigation nav = chronos->getNavigation();
            
            // Cập nhật dữ liệu từ thư viện theo thứ tự mới
            _navData.active = nav.active;
            _navData.isNavigation = nav.isNavigation;
            _navData.hasIcon = nav.hasIcon;
            _navData.iconCRC = nav.iconCRC;
            _navData.distance = nav.distance;
            _navData.duration = nav.duration;
            _navData.eta = nav.eta;
            _navData.directions = nav.directions;
            _navData.title = nav.title;
            _navData.speed = nav.speed;
            _isNavigating = nav.active;
            
            // Sao chép dữ liệu icon nếu có
            if (nav.hasIcon) {
                memcpy(_navData.icon, nav.icon, ICON_DATA_SIZE);
                Serial.println("Copied icon data from library cache");
            }
            
            Serial.println("\nFetched navigation data from library cache");
        }
        
        Serial.println("\n----- NAVIGATION DATA -----");
        Serial.printf("Active: %s\n", _navData.active ? "YES" : "NO");
        Serial.printf("HasIcon: %s\n", _navData.hasIcon ? "YES" : "NO");
        Serial.printf("IconCRC: 0x%08X\n", _navData.iconCRC);
        Serial.printf("Title: %s\n", _navData.title.c_str());
        Serial.printf("Distance: %s\n", _navData.distance.c_str());
        Serial.printf("Duration: %s\n", _navData.duration.c_str());
        Serial.printf("ETA: %s\n", _navData.eta.c_str());
        Serial.printf("Directions: %s\n", _navData.directions.c_str());
        Serial.println("===========================================");
    }
    
    static ChronosManager& getInstance() {
        static ChronosManager instance;
        return instance;
    }
};

#endif // CHRONOS_MANAGER_H
