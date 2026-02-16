#pragma once

#include "waveshare_epaper.h"

namespace esphome {
namespace waveshare_epaper {

class WaveshareEPaper2P13InGV2 : public WaveshareEPaper7C {
 public:
  void initialize() override;
  void display() override;
  void dump_config() override;
  void deep_sleep() override;
  
  // Support for fast initialization mode
  void set_fast_mode(bool fast_mode) { this->fast_mode_ = fast_mode; }

 protected:
  int get_width_internal() override;
  int get_height_internal() override;
  
  // Color mapping for 4-color display
  // Black = 0x0, White = 0x1, Yellow = 0x2, Red = 0x3
  uint8_t get_color_code(Color color);

 private:
  void init_display_();
  void turn_on_display_();
  bool fast_mode_{false};
};

}  // namespace waveshare_epaper
}  // namespace esphome
