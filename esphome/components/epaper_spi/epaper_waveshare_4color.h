#pragma once
#include "epaper_spi.h"
#include "epaper_spi_mono.h"

namespace esphome::epaper_spi {
/**
 * Waveshare 4-color e-paper display driver.
 * Handles 4 colors (Black, White, Red, Yellow) using 2 bits per pixel.
 */
class EpaperWaveshare4Color final : public EPaperMono {
 public:
  EpaperWaveshare4Color(const char *name, uint16_t width, uint16_t height, const uint8_t *init_sequence,
                        size_t init_sequence_length, const uint8_t *lut, size_t lut_length,
                        const uint8_t *partial_lut, uint16_t partial_lut_length)
      : EPaperMono(name, width, height, init_sequence, init_sequence_length),
        lut_(lut),
        lut_length_(lut_length),
        partial_lut_(partial_lut),
        partial_lut_length_(partial_lut_length) {
    // 4-color displays need 2 bits per pixel (4 pixels per byte)
    // Calculate row width in bytes (rounded up to nearest byte)
    this->row_width_4color_ = (width + 3) / 4;  // For 122: (122 + 3) / 4 = 31 bytes
    
    // CRITICAL FIX: Buffer must be row_width * height
    // This ensures we have enough space when rows are aligned to byte boundaries
    this->buffer_length_ = this->row_width_4color_ * height;  // 31 * 250 = 7750 bytes
  }

 protected:
  void draw_pixel_at(int x, int y, Color color) override;
  bool initialise(bool partial) override;
  void set_window() override;
  void refresh_screen(bool partial) override;
  bool transfer_data() override;

 private:
  uint8_t get_color_bits_(Color color);
  
  const uint8_t *lut_;
  size_t lut_length_;
  const uint8_t *partial_lut_;
  uint16_t partial_lut_length_;
  uint16_t row_width_4color_;  // row width in bytes for 4-color display
};
}  // namespace esphome::epaper_spi