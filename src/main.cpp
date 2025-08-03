#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <vector>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>  // Thư viện điều khiển Servo
#include <Wire.h>        // Thư viện I2C cho SR-04

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
  
  // Cấu hình cho Servo và SR-04
  constexpr uint8_t SERVO_PIN = 2;        // Chân điều khiển Servo
  constexpr uint8_t SERVO_MIN_ANGLE = 30; // Góc quét nhỏ nhất
  constexpr uint8_t SERVO_MAX_ANGLE = 140; // Góc quét lớn nhất
  constexpr uint8_t SERVO_STEP = 2;       // Tăng bước góc quét từ 2 lên 5
  constexpr uint16_t SERVO_DELAY_MS = 40; // Giảm thời gian chờ từ 50ms xuống 10ms
  
  // SR-04 qua I2C
  constexpr uint8_t SR04_I2C_ADDR = 0x57; // Địa chỉ I2C của SR-04 (có thể thay đổi)
  constexpr uint8_t SDA_PIN = 8;          // Chân SDA cho I2C
  constexpr uint8_t SCL_PIN = 9;          // Chân SCL cho I2C
  constexpr uint8_t OBSTACLE_THRESHOLD = 15; // Ngưỡng phát hiện vật cản (cm)
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
#include "video17.h"
#include "video18.h"
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
  
public:
  VideoPlayer() {
    // Sử dụng video15
    _videos = {
      &video17, &video18
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
    // (Vẫn giữ lại để tránh ảnh hưởng đến các phần khác của code)
    InputManager::getInstance().quickUpdate();
    
    // Không xử lý nút nhấn nữa vì chúng ta luôn phát video
    // handleInput(); - đã bỏ
    
    // Chỉ cập nhật frame nếu đang ở chế độ PLAYING (luôn đúng sau khi init)
    if (_currentMode == PlayerMode::PLAYING) {
      unsigned long currentTime = millis();
      
      if (currentTime - _lastFrameTime >= Config::FRAME_DELAY_MS) {
        _lastFrameTime = currentTime;
        
        // Hiển thị frame hiện tại
        DisplayManager::getInstance().drawFrame(getCurrentVideo(), _currentFrame);
        
        // Tăng chỉ số frame
        _currentFrame++;
        if (_currentFrame >= getCurrentVideo()->num_frames) {
          // Loop lại từ đầu khi hết video
          _currentFrame = 0;
          // Phát lại audio khi bắt đầu vòng lặp mới
          playCurrentAudio();
        }
        
        // Cập nhật nhanh input sau khi vẽ frame
        InputManager::getInstance().quickUpdate();
      }
    }
  }
  
private:
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
  // Phương thức để chuyển sang video khi phát hiện vật cản (video18)
  void switchToObstacleVideo() {
    // Chỉ chuyển nếu đang ở chế độ playing và không phải video 18
    if (_currentMode == PlayerMode::PLAYING && _currentIndex != 1) {
      _currentIndex = 1; // Video18 ở vị trí thứ 1 trong mảng
      _currentFrame = 0; // Bắt đầu từ frame đầu tiên
      playCurrentAudio(); // Phát audio mới (nếu có)
      Serial.println("Đã chuyển sang video phát hiện vật cản (video18)");
    }
  }
  
  // Phương thức để chuyển lại video bình thường khi vật cản đã được loại bỏ (video17)
  void switchToNormalVideo() {
    // Chỉ chuyển nếu đang ở chế độ playing và không phải video 17
    if (_currentMode == PlayerMode::PLAYING && _currentIndex != 0) {
      _currentIndex = 0; // Video17 ở vị trí thứ 0 trong mảng
      _currentFrame = 0; // Bắt đầu từ frame đầu tiên
      playCurrentAudio(); // Phát audio mới (nếu có)
      Serial.println("Đã chuyển lại video bình thường (video17)");
    }
  }
};

// ===== ULTRASONIC SENSOR MANAGER =====
class UltrasonicManager {
private:
  bool _initialized = false;
  uint8_t _i2cAddress = Config::SR04_I2C_ADDR;
  unsigned long _lastTriggerTime = 0;
  uint32_t _lastDistance = 0; // Thay đổi từ uint16_t sang uint32_t để chứa đủ 3 byte
  bool _measurementInProgress = false;
  
public:
  UltrasonicManager() {}
  
  void init() {
    // Khởi tạo I2C
    Wire.begin(Config::SDA_PIN, Config::SCL_PIN);
    _initialized = true;
    Serial.println("Ultrasonic I2C SR-04 initialized");
  }
  
  void update() {
    if (!_initialized) {
      return;
    }
    
    unsigned long currentTime = millis();
    
    // Bắt đầu đo mới nếu chưa đo hoặc sau 50ms
    if (!_measurementInProgress && (currentTime - _lastTriggerTime >= 50)) {
      // Trigger đo khoảng cách
      Wire.beginTransmission(_i2cAddress);
      Wire.write(0x01); // Command để bắt đầu đo khoảng cách
      Wire.endTransmission(true); // true để hoàn thành kết nối I2C
      
      _lastTriggerTime = currentTime;
      _measurementInProgress = true;
    }
    // Đọc kết quả nếu đã đủ thời gian (ít nhất 60ms sau khi trigger)
    else if (_measurementInProgress && (currentTime - _lastTriggerTime >= 150)) {
      Wire.requestFrom(_i2cAddress, (uint8_t)3); // Yêu cầu 3 byte
      while (Wire.available()) {
        uint32_t highByte = Wire.read();
        uint32_t midByte = Wire.read();
        uint32_t lowByte = Wire.read();
        // Tính toán khoảng cách từ 3 byte
        _lastDistance = (highByte << 16) | (midByte << 8) | lowByte;
      }
      
      _measurementInProgress = false;
    }
  }
  
  uint32_t getDistance() {
    return _lastDistance/10000; // Trả về kết quả đo mới nhất (cm)
  }
  
  static UltrasonicManager& getInstance() {
    static UltrasonicManager instance;
    return instance;
  }
};

// ===== SERVO SCANNER MANAGER =====
class ServoManager {
private:
  Servo _servo;
  int _currentAngle = Config::SERVO_MIN_ANGLE;
  int _direction = 1; // 1: tăng góc, -1: giảm góc
  unsigned long _lastMoveTime = 0;
  bool _scanning = false;
  bool _obstacleDetected = false;
  
public:
  ServoManager() {}
  
  void init() {
    _servo.attach(Config::SERVO_PIN);
    _servo.write(Config::SERVO_MIN_ANGLE);
    _currentAngle = Config::SERVO_MIN_ANGLE;
    _lastMoveTime = millis();
    _scanning = true;
    Serial.println("Servo scanner initialized");
  }
  
  void update() {
    if (!_scanning) {
      // Khi không quét, vẫn tiếp tục kiểm tra nếu vật cản đã được loại bỏ
      if (_obstacleDetected) {
        uint32_t distance = UltrasonicManager::getInstance().getDistance();
        
        // Nếu vật cản đã được loại bỏ, tiếp tục quét từ vị trí hiện tại
        if (distance == 0 || distance > Config::OBSTACLE_THRESHOLD) {
          _obstacleDetected = false;
          _scanning = true; // Tiếp tục quét ngay lập tức
          
          // Chuyển về video bình thường khi không còn vật cản
          VideoPlayer::getInstance().switchToNormalVideo();
        }
      }
      return;
    }
    
    unsigned long currentTime = millis();
    if (currentTime - _lastMoveTime >= Config::SERVO_DELAY_MS) {
      _lastMoveTime = currentTime;
      
      // Di chuyển servo theo bước đã định nghĩa - tốc độ radar
      _currentAngle += _direction * Config::SERVO_STEP;
      
      // Đảo hướng khi đạt giới hạn và tạo hiệu ứng quét radar
      if (_currentAngle >= Config::SERVO_MAX_ANGLE) {
        _currentAngle = Config::SERVO_MAX_ANGLE;
        _direction = -1;
        // Log để hiển thị đã đến giới hạn và đảo chiều
        // Serial.println("Đạt góc tối đa, đảo chiều quét");
      } else if (_currentAngle <= Config::SERVO_MIN_ANGLE) {
        _currentAngle = Config::SERVO_MIN_ANGLE;
        _direction = 1;
        // Log để hiển thị đã đến giới hạn và đảo chiều
        // Serial.println("Đạt góc tối thiểu, đảo chiều quét");
      }
      
      // Di chuyển servo đến góc mới
      _servo.write(_currentAngle);
      
      // Kiểm tra khoảng cách và phát hiện vật cản
      uint32_t distance = UltrasonicManager::getInstance().getDistance();
      
      // In khoảng cách của radar khi quét qua các góc 30, 60, 90, 120, 150
      if (_currentAngle % 30 == 0) {
        // Serial.printf("RADAR: Góc %d° - Khoảng cách: %d cm\n", _currentAngle, distance);
      }
      
      // Kiểm tra nếu phát hiện vật cản
      if (distance > 0 && distance <= Config::OBSTACLE_THRESHOLD) {
        if (!_obstacleDetected) {
          // Thay vì in log, chuyển sang video phát hiện vật cản (video18)
          VideoPlayer::getInstance().switchToObstacleVideo();
          
          _obstacleDetected = true;
          _scanning = false; // Dừng quét khi phát hiện vật cản
        }
      }
    }
  }
  
  void startScanning() {
    _scanning = true;
    _obstacleDetected = false;
    Serial.println("Servo scanning started");
  }
  
  void stopScanning() {
    _scanning = false;
    Serial.println("Servo scanning stopped");
  }
  
  bool isObstacleDetected() {
    return _obstacleDetected;
  }
  
  int getCurrentAngle() {
    return _currentAngle;
  }
  
  static ServoManager& getInstance() {
    static ServoManager instance;
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
  
  // Khởi tạo Ultrasonic I2C và Servo
  UltrasonicManager::getInstance().init();
  ServoManager::getInstance().init();
  
  Serial.println("Initialization complete - system ready in STOPPED mode");  
}

void loop() {
  // Vẫn giữ lại việc cập nhật đầu vào để các phần khác của code hoạt động bình thường
  InputManager::getInstance().quickUpdate();
  InputManager::getInstance().update();

  // Cập nhật video player (đã bỏ qua xử lý nút nhấn)
  VideoPlayer::getInstance().update();
  
  // Cập nhật quá trình đo khoảng cách (không chặn)
  UltrasonicManager::getInstance().update();
  
  // Cập nhật servo và phát hiện vật cản
  ServoManager::getInstance().update();
}