#ifndef LGFX_CONFIG_H
#define LGFX_CONFIG_H

#include <LovyanGFX.hpp>

// Cấu hình LovyanGFX cho ESP32-C3 với ST7789 240x240
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

#endif // LGFX_CONFIG_H
