#include "epaper_waveshare_4color.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.waveshare_4color";

// Color encoding for 4-color display:
// Black  = 00 (0)
// White  = 01 (1)
// Red    = 10 (2)  
// Yellow = 11 (3)

uint8_t EpaperWaveshare4Color::get_color_bits_(Color color) {
  // Convert ESPHome Color to 2-bit display color
  uint8_t r = color.r;
  uint8_t g = color.g;
  uint8_t b = color.b;
  
  // Check for white (all high)
  if (r > 200 && g > 200 && b > 200) {
    return 0b01;  // White
  }
  
  // Check for yellow (high red, high green, low blue)
  if (r > 200 && g > 200 && b < 100) {
    return 0b11;  // Yellow
  }
  
  // Check for red (high red, low green, low blue)
  if (r > 200 && g < 100 && b < 100) {
    return 0b10;  // Red
  }
  
  // Default to black
  return 0b00;  // Black
}

void EpaperWaveshare4Color::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->get_width_internal() || y < 0 || y >= this->get_height_internal()) {
    return;
  }
  
  // Calculate buffer position
  // Each byte contains 4 pixels (2 bits per pixel)
  uint32_t pixel_index = x + y * this->get_width_internal();
  uint32_t byte_index = pixel_index / 4;
  uint8_t bit_position = (pixel_index % 4) * 2;
  
  // Get the 2-bit color value
  uint8_t color_bits = this->get_color_bits_(color);
  
  // Clear the 2 bits at the position and set new value
  this->buffer_[byte_index] &= ~(0b11 << bit_position);
  this->buffer_[byte_index] |= (color_bits << bit_position);
  
  // Update dirty region
  if (x < this->x_low_)
    this->x_low_ = x;
  if (x >= this->x_high_)
    this->x_high_ = x + 1;
  if (y < this->y_low_)
    this->y_low_ = y;
  if (y >= this->y_high_)
    this->y_high_ = y + 1;
}

bool EpaperWaveshare4Color::initialise(bool partial) {
  // Call parent initialization
  EPaperMono::initialise(partial);
  
  // 4-color displays don't support partial refresh
  if (partial) {
    ESP_LOGW(TAG, "Partial refresh not supported on 4-color displays, using full refresh");
  }
  
  // Send LUT (even though 4-color displays typically don't use it in the same way,
  // we keep the API compatible with the base class)
  if (partial && this->partial_lut_ != nullptr) {
    this->cmd_data(0x32, this->partial_lut_, this->partial_lut_length_);
  } else if (this->lut_ != nullptr) {
    this->cmd_data(0x32, this->lut_, this->lut_length_);
  }
  
  return true;
}

void EpaperWaveshare4Color::init_buffer_(size_t length) {
  // For 4-color display, we need 2 bits per pixel instead of 1
  // So buffer size is (width * height) / 4 instead of (width * height) / 8
  size_t buffer_length = (this->get_width_internal() * this->get_height_internal() + 3) / 4;
  
  // Call parent's init_buffer_ with the correct size
  EPaperMono::init_buffer_(buffer_length);
  
  ESP_LOGD(TAG, "Initialized 4-color buffer: %u bytes for %dx%d display",
           buffer_length, this->get_width_internal(), this->get_height_internal());
}

}  // namespace esphome::epaper_spi
