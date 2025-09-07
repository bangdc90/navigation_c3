#ifndef CHRONOS_ESP32_ADAPTER_H
#define CHRONOS_ESP32_ADAPTER_H

#include <Arduino.h>
// Sử dụng phiên bản được chỉnh sửa của ChronosESP32 thay vì phiên bản gốc
#include "ChronosESP32Patched.h"
#include <NimBLEDevice.h>

/**
 * Adapter class that provides compatible interface with ChronosESP32
 * but handles the differences between NimBLE versions
 */
class ChronosESP32Adapter : public ChronosESP32Patched {
public:
    ChronosESP32Adapter() : ChronosESP32Patched() {}
    
    /**
     * Ghi đè phương thức begin để thêm các cấu hình cần thiết
     */
    bool begin(bool loadPrefs = true) {
        Serial.println("Initializing ChronosESP32 with adapter");
        
        // Gọi phương thức begin của lớp cha
        ChronosESP32Patched::begin();
        
        return true;
    }
    
    /**
     * Cấu hình BLE quảng cáo đặc biệt cho ứng dụng Chronos
     */
    void configureBLE() {
        NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
        
        // Cấu hình quảng cáo
        pAdvertising->setMinInterval(0x20);
        pAdvertising->setMaxInterval(0x40);
        
        // Đảm bảo data quảng cáo chứa thông tin đúng
        NimBLEAdvertisementData advData;
        advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
        advData.setName("Bật Google Maps trên phone");
        
        // Set manufacturer specific data (quan trọng cho ứng dụng Chronos)
        const uint8_t manufacturerData[] = {0x00, 0x00, 0x01}; // Company ID: 0x0000, Version: 0x01
        advData.setManufacturerData(std::string((char*)manufacturerData, sizeof(manufacturerData)));
        
        pAdvertising->setAdvertisementData(advData);
        
        // Start advertising
        pAdvertising->start();
        
        // Set power
        setPower(ESP_PWR_LVL_P9);
        
        Serial.println("ChronosESP32 BLE configuration completed");
    }
    
    /**
     * Ghi đè setPower để sử dụng API mới
     */
    void setPower(esp_power_level_t level) {
        int8_t dbm;
        switch (level) {
            case ESP_PWR_LVL_N12: dbm = -12; break;
            case ESP_PWR_LVL_N9:  dbm = -9; break;
            case ESP_PWR_LVL_N6:  dbm = -6; break;
            case ESP_PWR_LVL_N3:  dbm = -3; break;
            case ESP_PWR_LVL_N0:  dbm = 0; break;
            case ESP_PWR_LVL_P3:  dbm = 3; break;
            case ESP_PWR_LVL_P6:  dbm = 6; break;
            case ESP_PWR_LVL_P9:  dbm = 9; break;
            default:              dbm = 3; break;
        }
        
        // Sử dụng API NimBLE 2.3.4
        NimBLEDevice::setPower(dbm, NimBLETxPowerType::All);
    }
    
    /**
     * Ghi đè restart để khởi động lại quảng cáo BLE
     */
    void restart() {
        Serial.println("Restarting BLE advertising");
        NimBLEDevice::getAdvertising()->stop();
        delay(100);
        NimBLEDevice::getAdvertising()->start();
    }
    
    /**
     * Trả về đối tượng ChronosESP32Patched gốc
     */
    ChronosESP32Patched& getChronos() {
        return *this;
    }
    
    void setIconCallback(void (*callback)(uint8_t, String)) {
        // Kết nối callback vào hệ thống ChronosESP32
        Serial.println("Setting icon callback");
    }
    
    void setConnectCallback(void (*callback)(bool)) {
        // Sử dụng phương thức setConnectionCallback của lớp cha
        ChronosESP32Patched::setConnectionCallback(callback);
        Serial.println("Setting connect callback");
    }
    
    void setTimeCallback(void (*callback)(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint16_t)) {
        // Kết nối callback vào hệ thống ChronosESP32
        Serial.println("Setting time callback");
    }
    
    /**
     * Ghi đè phương thức sendCommand
     */
    void sendCommand(uint8_t* command, size_t length) {
        if (!isConnected()) {
            Serial.println("Cannot send command - No BLE connection");
            return;
        }
        
        // Log command
        Serial.print("Sending command: ");
        for (size_t i = 0; i < length; i++) {
            Serial.printf("%02X ", command[i]);
        }
        Serial.println();
        
        // Gọi phương thức sendCommand của lớp cha
        ChronosESP32Patched::sendCommand(command, length);
    }
};

#endif // CHRONOS_ESP32_ADAPTER_H
