#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <vector>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <Wire.h>        // Thư viện I2C cho MPU6050

// ===== CONFIG =====
namespace Config {
  // Hiển thị
  constexpr uint8_t DISPLAY_ROTATION = 0;  // Điều chỉnh rotation cho ST7789 1.3"
  constexpr uint8_t FRAME_DELAY_MS = 100;
  
  // GPIO và cảm biến
  constexpr uint8_t BUTTON_PIN = 1;
  constexpr uint8_t DEBOUNCE_TIME_MS = 15;       // Giảm để phản ứng nhanh hơn
  constexpr uint16_t HOLDING_TIME_MS = 400;
  
  // Thêm cấu hình cho việc tách biệt input processing
  constexpr bool PRIORITIZE_INPUT = true;          // Ưu tiên xử lý đầu vào
  constexpr unsigned long INPUT_CHECK_INTERVAL = 5; // Kiểm tra input thường xuyên hơn
  
  // Audio
  constexpr uint8_t AUDIO_VOLUME = 28; // Âm lượng (0-30)
  
  // Cấu hình cho MPU6050
  constexpr uint8_t SDA_PIN = 8;          // Chân SDA cho I2C
  constexpr uint8_t SCL_PIN = 9;          // Chân SCL cho I2C
  constexpr uint8_t MPU6050_I2C_ADDR = 0x68; // Địa chỉ I2C của MPU6050
  constexpr float SHAKE_THRESHOLD = 1.2;     // Ngưỡng phát hiện shake (g)
  constexpr unsigned long SHAKE_DETECTION_TIME = 1000; // Thời gian phát hiện shake (ms)
  constexpr unsigned long SHAKE_COOLDOWN_TIME = 1000;  // Thời gian chờ trước khi có thể phát hiện shake tiếp theo
}

// ===== CÁC KIỂU DỮ LIỆU =====
typedef struct _VideoInfo {
  const uint8_t* const* frames;
  const uint16_t* frames_size;
  uint16_t num_frames;
  uint8_t audio_idx;
} VideoInfo;

// ===== KHAI BÁO VIDEOS =====
// Tạm thời comment các video không có
// #include "video01.h"
// #include "video02.h"
// #include "video03.h"
// #include "video04.h"
// #include "video05.h"
// #include "video06.h"
// #include "video07.h"
// #include "video08.h"
// #include "video09.h"
// #include "video10.h"
// #include "video11.h"
// #include "video12.h"
// #include "video13.h"
// #include "video14.h"
// #include "video15.h"
// #include "video16.h"
// #include "video17.h"
// #include "video18.h"
#include "full1.h"
#include "chongmat1.h"
#include "xoadau1.h"

// ===== BUTTON STATE MACHINE =====
enum class ButtonState {
  IDLE,           // Không có nút nhấn
  PRESSED,        // Nút vừa được nhấn, đang chờ xem là nhấn thường hay giữ
  HELD,           // Nút đang được giữ
  RELEASED_SHORT, // Nút vừa được nhả sau khi nhấn ngắn
  RELEASED_HOLD   // Nút vừa được nhả sau khi giữ
};

// ===== PLAYER MODE =====
enum class PlayerMode {
  STOPPED,        // Chế độ dừng (màn hình đen)
  PLAYING,        // Chế độ phát video và audio
  PAUSED          // Chế độ tạm dừng (giữ màn hình hiện tại, không phát âm thanh)
};

// ===== MPU6050 SHAKE DETECTION MANAGER =====
class MPU6050Manager {
private:
  bool _initialized = false;
  uint8_t _i2cAddress = Config::MPU6050_I2C_ADDR;
  unsigned long _lastReadTime = 0;
  float _accelX = 0, _accelY = 0, _accelZ = 0;
  bool _shakeDetected = false;
  unsigned long _shakeStartTime = 0;
  unsigned long _lastShakeTime = 0; // Thời gian của lần shake cuối cùng
  
  // Bộ lọc để tính gia tốc tuyệt đối
  float _lastMagnitude = 1.0; // Khởi tạo với 1g (trọng lực)
  int _shakeCount = 0; // Đếm số lần shake trong khoảng thời gian
  unsigned long _shakeWindowStart = 0; // Thời điểm bắt đầu cửa sổ phát hiện shake
  
public:
  MPU6050Manager() {}
  
  void init() {
    // Khởi tạo I2C
    Wire.begin(Config::SDA_PIN, Config::SCL_PIN);
    
    // Kiểm tra kết nối với MPU6050
    Wire.beginTransmission(_i2cAddress);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      // Đánh thức MPU6050 (thoát khỏi chế độ sleep)
      writeRegister(0x6B, 0x00);
      
      // Cấu hình accelerometer range ±2g
      writeRegister(0x1C, 0x00);
      
      // Cấu hình Low Pass Filter
      writeRegister(0x1A, 0x03);
      
      delay(100);
      _initialized = true;
      Serial.println("MPU6050 initialized successfully");
    } else {
      Serial.printf("Failed to initialize MPU6050, error: %d\n", error);
    }
  }
  
  void update() {
    if (!_initialized) {
      return;
    }
    
    unsigned long currentTime = millis();
    
    // Đọc dữ liệu mỗi 20ms
    if (currentTime - _lastReadTime >= 20) {
      _lastReadTime = currentTime;
      
      // Đọc dữ liệu accelerometer
      readAccelData();
      
      // Tính độ lớn gia tốc tổng
      float magnitude = sqrt(_accelX * _accelX + _accelY * _accelY + _accelZ * _accelZ);
      
      // Tính sự thay đổi gia tốc (phát hiện shake)
      float deltaAccel = abs(magnitude - _lastMagnitude);
      _lastMagnitude = magnitude;
      
      // Kiểm tra cooldown period
      if (currentTime - _lastShakeTime < Config::SHAKE_COOLDOWN_TIME) {
        return; // Vẫn trong thời gian chờ
      }
      
      // Phát hiện shake nếu thay đổi gia tốc vượt ngưỡng
      if (deltaAccel > Config::SHAKE_THRESHOLD) {
        // Nếu chưa có cửa sổ shake nào, bắt đầu cửa sổ mới
        if (_shakeCount == 0) {
          _shakeWindowStart = currentTime;
        }
        
        _shakeCount++;
        Serial.printf("Shake event %d! Delta: %.2f g\n", _shakeCount, deltaAccel);
        
        // Kiểm tra nếu có đủ shake trong khoảng thời gian quy định
        if (_shakeCount >= 3 && (currentTime - _shakeWindowStart) <= Config::SHAKE_DETECTION_TIME) {
          if (!_shakeDetected) {
            _shakeDetected = true;
            _lastShakeTime = currentTime;
            Serial.printf("STRONG SHAKE DETECTED! %d shakes in %lu ms\n", 
                         _shakeCount, currentTime - _shakeWindowStart);
          }
        }
      }
      
      // Reset cửa sổ shake nếu quá thời gian quy định
      if (_shakeCount > 0 && (currentTime - _shakeWindowStart) > Config::SHAKE_DETECTION_TIME) {
        _shakeCount = 0;
        Serial.println("Shake window reset");
      }
    }
  }
  
  bool isShakeDetected() {
    bool detected = _shakeDetected;
    if (detected) {
      _shakeDetected = false; // Reset sau khi đọc
    }
    return detected;
  }
  
  void getAcceleration(float &x, float &y, float &z) {
    x = _accelX;
    y = _accelY;
    z = _accelZ;
  }
  
private:
  void writeRegister(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(_i2cAddress);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
  }
  
  uint8_t readRegister(uint8_t reg) {
    Wire.beginTransmission(_i2cAddress);
    Wire.write(reg);
    Wire.endTransmission(false);
    Wire.requestFrom(_i2cAddress, (uint8_t)1);
    return Wire.read();
  }
  
  void readAccelData() {
    Wire.beginTransmission(_i2cAddress);
    Wire.write(0x3B); // ACCEL_XOUT_H register
    Wire.endTransmission(false);
    Wire.requestFrom(_i2cAddress, (uint8_t)6);
    
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();
    
    // Chuyển đổi sang đơn vị g (±2g range)
    _accelX = ax / 16384.0;
    _accelY = ay / 16384.0;
    _accelZ = az / 16384.0;
  }
  
public:
  static MPU6050Manager& getInstance() {
    static MPU6050Manager instance;
    return instance;
  }
};

// Cấu hình LovyanGFX cho ESP32-C3 với ST7735 128x160
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

public:
  LGFX(void)
  {
    // Cấu hình SPI Bus
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 3;
      cfg.freq_write = 40000000;    // Tăng tốc độ SPI cho ST7789
      cfg.freq_read  = 20000000;
      cfg.spi_3wire  = true;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO; // Kích hoạt DMA
      cfg.pin_sclk = 4;    // SCK (TFT_SCLK)
      cfg.pin_mosi = 6;    // SDA (TFT_MOSI)
      cfg.pin_miso = -1;   // Không sử dụng
      cfg.pin_dc   = 3;    // DC/RS (TFT_DC)
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // Cấu hình Panel cho màn hình ST7789 1.3" 240x240
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = -1;     // Sử dụng CS cho ổn định hơn
      cfg.pin_rst          = 10;    // RST (TFT_RST)
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;   // Kích thước vật lý 240x240
      cfg.panel_height     = 240;   // Kích thước vật lý 240x240
      cfg.offset_x         = 0;     // ST7789 1.3" thường có offset 0
      cfg.offset_y         = 0;     // ST7789 1.3" thường có offset 0
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = false;
      cfg.invert           = true;  // ST7789 thường cần đảo ngược màu
      cfg.rgb_order        = false;  // RGB
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;

      _panel_instance.config(cfg);
    }

    setPanel(&_panel_instance);
  }
};

// ===== QUẢN LÝ HIỂN THỊ =====
class DisplayManager {
private:
  LGFX _tft;
  
public:
  DisplayManager() : _tft() {}
  
  void init() {
    // Cấu hình backlight là chân GPIO 7
    pinMode(7, OUTPUT);
    digitalWrite(7, HIGH); // BLK ON
    
    _tft.init();
    _tft.setRotation(Config::DISPLAY_ROTATION);
    _tft.fillScreen(TFT_BLACK);
  }
  
  LGFX* getTft() { return &_tft; }
  
  // Điều khiển đèn nền màn hình (GPIO 7)
  void setBacklight(bool on) {
    digitalWrite(7, on ? HIGH : LOW);
  }
  
  void drawFrame(const VideoInfo* video, uint16_t frameIndex) {
    const uint8_t* jpg_data = (const uint8_t*)pgm_read_ptr(&video->frames[frameIndex]);
    uint16_t jpg_size = pgm_read_word(&video->frames_size[frameIndex]);
  
    _tft.drawJpg(jpg_data, jpg_size, 0, 0);
  }
  
  static DisplayManager& getInstance() {
    static DisplayManager instance;
    return instance;
  }
};

// ===== QUẢN LÝ AUDIO =====
class AudioManager {
private:
  DFRobotDFPlayerMini _dfPlayer;
  HardwareSerial _dfSerial;
  bool _isPlaying = false;
  uint8_t _currentTrack = 0;
  unsigned long _lastCommandTime = 0;
  static constexpr unsigned long MIN_COMMAND_INTERVAL = 50;

public:
  AudioManager() : _dfSerial(1) {} // Sử dụng HardwareSerial 1 cho ESP32-C3
  
  void init() {
    // Khởi tạo Serial cho DFPlayer (RX: GPIO 21, TX: GPIO 20)
    _dfSerial.begin(9600, SERIAL_8N1, 21, 20);
    
    Serial.println("Initializing DFPlayer...");
    if (!_dfPlayer.begin(_dfSerial)) {
      Serial.println("Failed to initialize DFPlayer!");
    } else {
      Serial.println("DFPlayer Mini initialized!");
      _dfPlayer.setTimeOut(500);
      _dfPlayer.volume(Config::AUDIO_VOLUME);
      _dfPlayer.EQ(DFPLAYER_EQ_NORMAL);
    }
  }
  
  void play(uint8_t trackNumber) {
    if (millis() - _lastCommandTime < MIN_COMMAND_INTERVAL) {
      delay(MIN_COMMAND_INTERVAL);
    }
    
    // Dừng audio hiện tại trước khi phát mới
    if (_isPlaying) {
      _dfPlayer.stop();
      delay(50);
    }
    _dfPlayer.play(trackNumber);
    _currentTrack = trackNumber;
    _isPlaying = true;
    _lastCommandTime = millis();
    
    Serial.printf("Playing audio track: %d\n", trackNumber);
  }
  
  void stop() {
    if (!_isPlaying) {
      return;
    }
    
    if (millis() - _lastCommandTime < MIN_COMMAND_INTERVAL) {
      delay(MIN_COMMAND_INTERVAL);
    }
    
    _dfPlayer.stop();
    _isPlaying = false;
    _lastCommandTime = millis();
  }
  
  static AudioManager& getInstance() {
    static AudioManager instance;
    return instance;
  }
};

// ===== INPUT MANAGER =====
class InputManager {
private:
  ButtonState _buttonState = ButtonState::IDLE;
  unsigned long _buttonPressTime = 0;
  unsigned long _lastButtonReleaseTime = 0;
  int _lastReading = HIGH;
  unsigned long _lastDebounceTime = 0;
  unsigned long _lastUpdateTime = 0;
  
  // Trạng thái nút vật lý
  bool _physicalButtonState = false; // LOW (nhấn) = true, HIGH (nhả) = false
  
public:
  void init() {
    pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
    _lastUpdateTime = millis();
  }
  
  // Cập nhật nhanh - chỉ đọc trạng thái nút
  void quickUpdate() {
    unsigned long now = millis();
    if (now - _lastUpdateTime >= Config::INPUT_CHECK_INTERVAL) {
      _lastUpdateTime = now;
      
      // Đọc trạng thái nút vật lý (đảo ngược vì INPUT_PULLUP)
      bool newButtonState = (digitalRead(Config::BUTTON_PIN) == LOW);
      
      // Nếu trạng thái thay đổi, cập nhật thời gian debounce
      if (newButtonState != _physicalButtonState) {
        _lastDebounceTime = now;
      }
      _physicalButtonState = newButtonState;
    }
  }
  
  // Cập nhật đầy đủ - xử lý state machine
  void update() {
    quickUpdate(); // Đảm bảo có dữ liệu mới nhất
    processButton();
    // Đã loại bỏ phần updateDoubleClickState()
  }
  
  ButtonState getButtonState() {
    ButtonState currentState = _buttonState;
    
    // Reset transient states
    if (_buttonState == ButtonState::RELEASED_SHORT || _buttonState == ButtonState::RELEASED_HOLD) {
      _buttonState = ButtonState::IDLE;
    }
    
    return currentState;
  }

private:
  // Xử lý trạng thái nút dựa trên _physicalButtonState
  void processButton() {
    unsigned long now = millis();
    
    // Chỉ xử lý sau khi vượt qua thời gian debounce
    if (now - _lastDebounceTime <= Config::DEBOUNCE_TIME_MS) {
      return;
    }
    
    // Xử lý trạng thái nút dựa trên _physicalButtonState
    if (_physicalButtonState) {  // Nút đang được nhấn
      if (_buttonState == ButtonState::IDLE) {
        _buttonState = ButtonState::PRESSED;
        _buttonPressTime = now;
        Serial.println("Button pressed");
      }
      else if (_buttonState == ButtonState::PRESSED && 
               now - _buttonPressTime >= Config::HOLDING_TIME_MS) {
        _buttonState = ButtonState::HELD;
        Serial.println("Button hold detected");
      }
    }
    else {  // Nút đang được nhả
      if (_buttonState == ButtonState::PRESSED) {
        if (now - _buttonPressTime < Config::HOLDING_TIME_MS) {
          _buttonState = ButtonState::RELEASED_SHORT;
          _lastButtonReleaseTime = now;
          Serial.println("Button released (short press)");
        } else {
          _buttonState = ButtonState::RELEASED_HOLD;
          _lastButtonReleaseTime = now;
          Serial.println("Button released (after hold)");
        }
      }
      else if (_buttonState == ButtonState::HELD) {
        _buttonState = ButtonState::RELEASED_HOLD;
        _lastButtonReleaseTime = now;
        Serial.println("Button released (after hold)");
      }
    }
  }
  
  // Đã loại bỏ phương thức updateDoubleClickState()

public:
  static InputManager& getInstance() {
    static InputManager instance;
    return instance;
  }
};

// ===== VIDEO PLAYER =====
class VideoPlayer {
private:
  std::vector<VideoInfo*> _videos;
  uint8_t _currentIndex = 0;
  uint16_t _currentFrame = 0;
  PlayerMode _currentMode = PlayerMode::STOPPED; // Bắt đầu ở chế độ dừng
  unsigned long _lastFrameTime = 0;
  bool _playingSpecialVideo = false; // Flag để theo dõi video đặc biệt
  uint16_t _savedFrame = 0; // Lưu vị trí frame khi bị ngắt quãng
  bool _playingButtonHoldVideo = false; // Flag để theo dõi video button hold
  
public:
  VideoPlayer() {
    // Sử dụng full1 (video chính), chongmat1 (video shake), và xoadau1 (video button hold)
    _videos = {
      &full1,      // Index 0: Video chính
      &chongmat1,  // Index 1: Video shake
      &xoadau1     // Index 2: Video button hold
    };
    
    // Bắt đầu với video đầu tiên
    _currentIndex = 0;
    
    Serial.printf("Loaded %d video files\n", _videos.size());
    for(int i = 0; i < _videos.size(); i++) {
      Serial.printf("Video %d: %d frames, audio track %d\n", 
                   i, _videos[i]->num_frames, _videos[i]->audio_idx);
    }
  }
  
  void init() {
    // Khởi tạo với màn hình đen, sau đó tự động bắt đầu phát video
    DisplayManager::getInstance().getTft()->fillScreen(TFT_BLACK);
    
    // Tự động bắt đầu phát video ngay khi khởi động, không cần nhấn nút
    _currentMode = PlayerMode::PLAYING;
    _currentIndex = 0; // Bắt đầu với video17 (bình thường)
    _currentFrame = 0;
    _lastFrameTime = millis();
    DisplayManager::getInstance().setBacklight(true); // Bật đèn nền
    playCurrentAudio(); // Phát audio nếu có
    
    Serial.println("Player initialized and automatically started playback");
  }
  
  void update() {
    // Đảm bảo luôn cập nhật nhanh đầu vào trước khi làm bất cứ việc gì khác
    InputManager::getInstance().quickUpdate();
    
    // Kiểm tra shake detection từ MPU6050 (ưu tiên cao nhất)
    handleShakeDetection();
    
    // Xử lý button input (ưu tiên thấp hơn shake)
    handleButtonInput();
    
    // Chỉ cập nhật frame nếu đang ở chế độ PLAYING
    if (_currentMode == PlayerMode::PLAYING) {
      unsigned long currentTime = millis();
      
      if (currentTime - _lastFrameTime >= Config::FRAME_DELAY_MS) {
        _lastFrameTime = currentTime;
        
        // Hiển thị frame hiện tại
        DisplayManager::getInstance().drawFrame(getCurrentVideo(), _currentFrame);
        
        // Tăng chỉ số frame
        _currentFrame++;
        if (_currentFrame >= getCurrentVideo()->num_frames) {
          // Khi video kết thúc
          if (_playingSpecialVideo) {
            // Nếu đang phát video đặc biệt (chongmat1), quay lại video chính
            _playingSpecialVideo = false;
            _currentIndex = 0; // Quay lại full1
            _currentFrame = _savedFrame; // Quay lại vị trí frame đã lưu
            playCurrentAudio();
            Serial.printf("Video chongmat1 kết thúc, quay lại video full1 tại frame %d\n", _savedFrame);
          } else if (_playingButtonHoldVideo) {
            // Nếu đang phát video button hold (xoadau1), loop lại video này
            _currentFrame = 0;
            playCurrentAudio();
            Serial.println("Video xoadau1 loop lại");
          } else {
            // Nếu đang phát video chính, loop lại từ đầu
            _currentFrame = 0;
            playCurrentAudio();
          }
        }
        
        // Cập nhật nhanh input sau khi vẽ frame
        InputManager::getInstance().quickUpdate();
      }
    }
  }
  
private:
  // Xử lý phát hiện shake (ưu tiên cao nhất)
  void handleShakeDetection() {
    if (MPU6050Manager::getInstance().isShakeDetected()) {
      // Shake có ưu tiên cao nhất - có thể ngắt cả video button hold
      if (!_playingSpecialVideo && _currentMode == PlayerMode::PLAYING) {
        // Lưu vị trí frame hiện tại
        if (_playingButtonHoldVideo) {
          // Nếu đang phát video button hold, vẫn lưu frame của video chính
          // (frame đã được lưu khi bắt đầu button hold)
          Serial.println("Shake ngắt video button hold");
        } else {
          // Nếu đang phát video chính, lưu frame hiện tại
          _savedFrame = _currentFrame;
        }
        
        // Tắt flag button hold nếu đang bật và chuyển sang shake
        _playingButtonHoldVideo = false;
        _playingSpecialVideo = true;
        _currentIndex = 1; // Chuyển sang chongmat1
        _currentFrame = 0;
        playCurrentAudio();
        Serial.printf("Shake detected! Lưu frame %d, chuyển sang video chongmat1\n", _savedFrame);
      }
    }
  }
  
  // Xử lý button input (ưu tiên thấp hơn shake)
  void handleButtonInput() {
    auto& inputManager = InputManager::getInstance();
    ButtonState buttonState = inputManager.getButtonState();
    
    // Xử lý button hold (chỉ khi không đang phát video shake)
    if (buttonState == ButtonState::HELD) {
      // Chuyển sang video xoadau1 nếu chưa đang phát và không đang phát video shake
      if (!_playingButtonHoldVideo && !_playingSpecialVideo && _currentMode == PlayerMode::PLAYING) {
        // Lưu vị trí frame hiện tại của video chính
        _savedFrame = _currentFrame;
        
        _playingButtonHoldVideo = true;
        _currentIndex = 2; // Chuyển sang xoadau1
        _currentFrame = 0;
        playCurrentAudio();
        Serial.printf("Button hold detected! Lưu frame %d, chuyển sang video xoadau1\n", _savedFrame);
      }
    }
    
    // Xử lý button release từ hold (chỉ khi không đang phát video shake)
    if (buttonState == ButtonState::RELEASED_HOLD) {
      // Quay lại video chính nếu đang phát video button hold và không đang phát video shake
      if (_playingButtonHoldVideo && !_playingSpecialVideo && _currentMode == PlayerMode::PLAYING) {
        _playingButtonHoldVideo = false;
        _currentIndex = 0; // Quay lại full1
        _currentFrame = _savedFrame; // Quay lại vị trí frame đã lưu
        playCurrentAudio();
        Serial.printf("Button released from hold! Quay lại video full1 tại frame %d\n", _savedFrame);
      }
    }
  }
  
  // Xử lý đầu vào
  void handleInput() {
    auto& inputManager = InputManager::getInstance();
    ButtonState buttonState = inputManager.getButtonState();
    
    // Xử lý nút nhấn theo mô hình đơn giản
    if (buttonState == ButtonState::RELEASED_SHORT) {
      // Xử lý nhấn nút theo mô hình đơn giản: STOPPED -> PLAYING -> STOPPED
      switch (_currentMode) {
        case PlayerMode::STOPPED:
          // Khi đang dừng, nhấn nút sẽ bắt đầu phát
          _currentMode = PlayerMode::PLAYING;
          _currentIndex = 0; // Luôn bắt đầu với video17 (bình thường)
          _currentFrame = 0; // Bắt đầu từ frame đầu tiên
          _lastFrameTime = millis(); // Reset thời gian để phát ngay lập tức
          DisplayManager::getInstance().setBacklight(true); // Bật đèn nền
          playCurrentAudio(); // Phát audio nếu có
          Serial.println("Bắt đầu phát video");
          break;
          
        case PlayerMode::PLAYING:
          // Khi đang phát, nhấn nút sẽ dừng lại
          _currentMode = PlayerMode::STOPPED;
          AudioManager::getInstance().stop(); // Dừng audio
          clearScreen(); // Xóa màn hình về màu đen
          Serial.println("Dừng phát video");
          break;
          
        case PlayerMode::PAUSED:
          // Trường hợp dự phòng, nếu sau này cần thêm chế độ pause
          _currentMode = PlayerMode::STOPPED;
          clearScreen();
          Serial.println("Dừng phát video");
          break;
      }
    }
  }
  
  // Phát audio cho video hiện tại
  void playCurrentAudio() {
    uint8_t audioTrack = getCurrentVideo()->audio_idx;
    // Chỉ phát audio nếu audio_idx khác 0
    if (audioTrack != 0) {
      AudioManager::getInstance().play(audioTrack);
    } else {
      // Nếu audio_idx = 0, dừng audio đang phát (nếu có)
      AudioManager::getInstance().stop();
      Serial.println("Không có audio để phát (audio_idx = 0)");
    }
  }
  
  // Lấy video hiện tại
  VideoInfo* getCurrentVideo() const {
    return _videos[_currentIndex];
  }
  
  // Xóa màn hình về màu đen và tắt đèn nền
  void clearScreen() {
    DisplayManager::getInstance().getTft()->fillScreen(TFT_BLACK);
    DisplayManager::getInstance().setBacklight(false); // Tắt backlight khi màn hình đen
  }


public:
  static VideoPlayer& getInstance() {
    static VideoPlayer instance;
    return instance;
  }
};

// ===== PROGRAM ENTRY POINTS =====
void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  
  // Thêm mã debug cho màn hình
  Serial.println("Initializing display...");
  DisplayManager::getInstance().init();
  
  LGFX* tft = DisplayManager::getInstance().getTft();
  Serial.printf("Display initialized: %dx%d pixels\n", tft->width(), tft->height());
  
  AudioManager::getInstance().init();
  InputManager::getInstance().init();
  VideoPlayer::getInstance().init();
  
  // Khởi tạo MPU6050 cho phát hiện shake
  MPU6050Manager::getInstance().init();
  
  Serial.println("Initialization complete - system ready in STOPPED mode");  
}

void loop() {
  // Vẫn giữ lại việc cập nhật đầu vào để các phần khác của code hoạt động bình thường
  InputManager::getInstance().quickUpdate();
  InputManager::getInstance().update();

  // Cập nhật MPU6050 để phát hiện shake
  MPU6050Manager::getInstance().update();

  // Cập nhật video player (bao gồm xử lý shake detection)
  VideoPlayer::getInstance().update();
}