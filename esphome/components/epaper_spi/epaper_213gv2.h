#pragma once

#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * Waveshare 2.13" 4-color (Black/White/Yellow/Red) E-Paper Display V2
 * Resolution: 122x250
 * Color depth: 2 bits per pixel (4 colors)
 */
class EPaper213GV2 : public EPaperBase {
 public:
  EPaper213GV2(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
               size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length, DISPLAY_TYPE_COLOR) {
    // 4 pixels per byte (2 bits per pixel), rounded up
    this->buffer_length_ = ((width + 3) / 4) * height;
    this->row_width_ = (width + 3) / 4;  // Override row width for 2-bit format
  }

  void fill(Color color) override;
  DisplayType get_display_type() override { return DISPLAY_TYPE_COLOR; }

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  bool transfer_data() override;
  void refresh_screen(bool partial) override;
  void power_on() override;
  void power_off() override;
  void deep_sleep() override;
  bool initialise(bool partial) override;

 private:
  uint8_t color_to_4color_(Color color);
  void init_fast_();
  
  // Color definitions (2 bits per pixel)
  static constexpr uint8_t COLOR_BLACK = 0x0;
  static constexpr uint8_t COLOR_WHITE = 0x1;
  static constexpr uint8_t COLOR_YELLOW = 0x2;
  static constexpr uint8_t COLOR_RED = 0x3;
};

}  // namespace esphome::epaper_spi