#pragma once
#include "epaper_spi.h"
#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {

/**
 * An epaper display for Waveshare 4-color (BWRY) displays.
 * 
 * These displays use 2 bits per pixel to represent 4 colors:
 * - Black (00)
 * - White (01)  
 * - Red (10)
 * - Yellow (11)
 * 
 * Based on EPaperMono but with 4-color buffer handling.
 */
class EpaperWaveshare4Color final : public EPaperMono {
 public:
  EpaperWaveshare4Color(const char *name, uint16_t width, uint16_t height, 
                        const uint8_t *init_sequence, size_t init_sequence_length,
                        const uint8_t *lut, size_t lut_length, 
                        const uint8_t *partial_lut, uint16_t partial_lut_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length),
        lut_(lut),
        lut_length_(lut_length),
        partial_lut_(partial_lut),
        partial_lut_length_(partial_lut_length) {}

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  
 protected:
  bool initialise(bool partial) override;
  void init_buffer_(size_t length) override;
  
 private:
  // Helper to get 2-bit color value from Color object
  uint8_t get_color_bits_(Color color);
  
  const uint8_t *lut_;
  size_t lut_length_;
  const uint8_t *partial_lut_;
  uint16_t partial_lut_length_;
};

}  // namespace esphome::epaper_spi
