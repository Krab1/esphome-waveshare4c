#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display_buffer.h"
#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

class WaveshareEPaper2P13InGV2 : public WaveshareEPaperBase {
 public:
  void initialize() override;
  void display() override;
  void dump_config() override;
  void deep_sleep() override;
  
  void fill(Color color) override;
  
  display::DisplayType get_display_type() override { 
    return display::DisplayType::DISPLAY_TYPE_COLOR; 
  }
  
  void set_full_update_every(uint32_t full_update_every) { 
    this->full_update_every_ = full_update_every; 
  }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  uint32_t get_buffer_length_() override;
  int get_width_internal() override;
  int get_height_internal() override;
  
  void reset_() override {
    if (this->reset_pin_ != nullptr) {
      this->reset_pin_->digital_write(true);
      delay(200);  // NOLINT
      this->reset_pin_->digital_write(false);
      delay(2);
      this->reset_pin_->digital_write(true);
      delay(200);  // NOLINT
    }
  }
  
  void turn_on_display_();
  void init_fast_();
  uint8_t color_to_4color_(Color color);
  
  uint32_t idle_timeout_() override { return 10000u; }  // 10 seconds timeout for fast refresh
  
  uint32_t full_update_every_{30};
  uint32_t at_update_{0};
  bool fast_mode_enabled_{false};  // Track if we've switched to fast mode
};

}  // namespace waveshare_epaper
}  // namespace esphome