#pragma once
#include "epaper_spi.h"

namespace esphome::epaper_spi {

/**
 * An epaper display for Waveshare 4-color (BWRY) displays.
 * These displays use 2 bits per pixel to represent 4 colors:
 * - Black (00)
 * - White (01)  
 * - Red (10)
 * - Yellow (11)
 */
class EpaperWaveshare4Color final : public EPaperBase {
 public:
  EpaperWaveshare4Color(const char *name, uint16_t width, uint16_t height, 
                        const uint8_t *init_sequence, size_t init_sequence_length)
      : EPaperBase(name, width, height, init_sequence, init_sequence_length) {}

  void draw_pixel(int16_t x, int16_t y, Color color) override;
  
 protected:
  bool initialise(bool partial) override;
  void set_window() override;
  void refresh_screen(bool partial) override;
  void deep_sleep() override;
  void write_init_buffer_() override;
  void write_buffer_() override;
  
 private:
  // Helper to get 2-bit color value from Color object
  uint8_t get_color_bits_(Color color);
};

}  // namespace esphome::epaper_spi
