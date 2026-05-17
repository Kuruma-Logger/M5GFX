/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)
/----------------------------------------------------------------------------*/
#pragma once

#if __has_include(<soc/soc_caps.h>)
#include <soc/soc_caps.h> 
#if SOC_MIPI_DSI_SUPPORTED

#include <esp_lcd_mipi_dsi.h>
#include <esp_ldo_regulator.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../panel/Panel_FrameBufferBase.hpp"

#include "Bus_DSI.hpp"

namespace lgfx
{
 inline namespace v1
 {
//----------------------------------------------------------------------------
  struct Bus_DSI;

  struct Panel_DSI : public Panel_FrameBufferBase
  {
  public:

    struct config_detail_t
    {
      void* buffer = nullptr;
      void* buffer_back = nullptr;  // second framebuffer for double-buffering
      uint32_t buffer_length = 0;

      uint16_t dpi_freq_mhz = 60;
      uint16_t hsync_back_porch = 1;
      uint16_t hsync_pulse_width = 1;
      uint16_t hsync_front_porch = 1;
      uint16_t vsync_back_porch = 1;
      uint16_t vsync_pulse_width = 1;
      uint16_t vsync_front_porch = 1;
    };

    bool init(bool use_reset) override;

    color_depth_t setColorDepth(color_depth_t depth) override;

    /// Swap draw/display framebuffers (double-buffering).
    /// Call after endWrite() to present the completed frame.
    /// Returns the pointer to the NEW draw buffer.
    void* swapFrameBuffer(void);

    /// Kuruma-Logger fork addition (Tab5 atomic frame mode):
    /// Copy an external RGB565 buffer into the current draw framebuffer
    /// using esp_lcd_panel_draw_bitmap. The DPI driver routes this through
    /// DMA2D (configured via use_dma2d=true at init), so the copy runs on
    /// hardware and does not consume CPU. After the copy, the DPI controller
    /// flips to the just-written buffer at the next VSync, presenting the
    /// frame atomically.
    ///
    /// `src` must point to panel_width * panel_height * bytes_per_pixel bytes
    /// of pixel data in PSRAM, RGB565 format, contiguous.
    ///
    /// This replaces the pushSprite + swapFrameBuffer pattern that doubled
    /// PSRAM bus pressure during per-pixel CPU memcpy.
    void blitFromBuffer(const void* src);

    /// True when num_fbs >= 2 and both buffers were allocated.
    bool hasDoubleBuffer(void) const { return _config_detail.buffer_back != nullptr; }

    const config_detail_t& config_detail(void) const { return _config_detail; }
    void config_detail(const config_detail_t& config_detail) { _config_detail = config_detail; };

    void setInvert(bool invert) override;
    void setSleep(bool flg_sleep) override;
    void setPowerSave(bool flg_idle) override;

    Bus_DSI* getBusDSI(void) const
    {
      auto b = getBus();
      return (b && b->busType() == bus_type_t::bus_dsi)
           ? static_cast<Bus_DSI*>(b)
           : nullptr;
    }

  protected:

    static constexpr uint8_t CMD_SLPIN   = 0x10;
    static constexpr uint8_t CMD_SLPOUT  = 0x11;
    static constexpr uint8_t CMD_INVOFF  = 0x20;
    static constexpr uint8_t CMD_INVON   = 0x21;
    static constexpr uint8_t CMD_DISPOFF = 0x28;
    static constexpr uint8_t CMD_DISPON  = 0x29;
    static constexpr uint8_t CMD_MADCTL  = 0x36;
    static constexpr uint8_t CMD_IDMOFF  = 0x38;
    static constexpr uint8_t CMD_IDMON   = 0x39;
    static constexpr uint8_t CMD_COLMOD  = 0x3A;

    virtual const uint8_t* getInitParams(size_t listno) const { return nullptr; }
    virtual size_t getInitDelay(size_t listno) const { return 0; }

    bool write_params(uint32_t cmd, const uint8_t* data = nullptr, size_t length = 0);

    bool init_dpi(Bus_DSI* bus);
    bool init_panel(void);

    config_detail_t _config_detail;

    esp_lcd_panel_handle_t _disp_panel_handle = nullptr;

    /// Kuruma-Logger fork: gates back-to-back blitFromBuffer calls until the
    /// DMA2D copy from the previous frame has completed. The IDF DPI driver
    /// returns ESP_ERR_INVALID_STATE if its internal draw_sem is still held;
    /// without our own wait, fast-cadence blits silently drop and the FB
    /// shows torn / mixed content.
    SemaphoreHandle_t _trans_done_sem = nullptr;
    static bool IRAM_ATTR onTransDoneIsr(esp_lcd_panel_handle_t panel,
                                         esp_lcd_dpi_panel_event_data_t* edata,
                                         void* user_ctx);
  };

//----------------------------------------------------------------------------
 }
}

#endif
#endif
