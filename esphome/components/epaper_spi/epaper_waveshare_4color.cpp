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
  // For 4-color displays, we need to detect red and yellow
  
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

void EpaperWaveshare4Color::draw_pixel(int16_t x, int16_t y, Color color) {
  if (x < 0 || x >= this->get_width() || y < 0 || y >= this->get_height()) {
    return;
  }
  
  // Calculate buffer position
  // Each byte contains 4 pixels (2 bits per pixel)
  uint32_t pixel_index = x + y * this->get_width();
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
  EPaperBase::initialise(partial);
  
  // 4-color displays don't support partial refresh
  if (partial) {
    ESP_LOGW(TAG, "Partial refresh not supported on 4-color displays, using full refresh");
  }
  
  // Wait for the display to be ready after init sequence
  this->next_delay_ = 10;
  this->send_red_ = false;
  
  return true;
}

void EpaperWaveshare4Color::set_window() {
  // Set RAM X address (horizontal)
  // Address is in bytes (width/8), but for 4-color we need width/4
  uint8_t x_start = this->x_low_ / 4;
  uint8_t x_end = (this->x_high_ + 3) / 4 - 1;
  
  this->cmd_data(0x44, {x_start, x_end});  // Set RAM X address start/end
  this->cmd_data(0x4E, {x_start});         // Set RAM X address counter
  
  // Set RAM Y address (vertical) 
  uint16_t y_start = this->y_low_;
  uint16_t y_end = this->y_high_ - 1;
  
  this->cmd_data(0x45, {
    (uint8_t)(y_start & 0xFF),
    (uint8_t)(y_start >> 8),
    (uint8_t)(y_end & 0xFF),
    (uint8_t)(y_end >> 8)
  });
  this->cmd_data(0x4F, {
    (uint8_t)(y_start & 0xFF),
    (uint8_t)(y_start >> 8)
  });
  
  ESP_LOGV(TAG, "Set window X: %u-%u, Y: %u-%u", this->x_low_, this->x_high_, 
           this->y_low_, this->y_high_);
}

void EpaperWaveshare4Color::write_init_buffer_() {
  // For 4-color display, write initial buffer as all white
  this->command(0x24);  // Write RAM command
  
  uint32_t buffer_size = (this->get_width() * this->get_height() + 3) / 4;
  for (uint32_t i = 0; i < buffer_size; i++) {
    this->data(0x55);  // 0x55 = 01010101 = all white pixels
  }
}

void EpaperWaveshare4Color::write_buffer_() {
  // Write the image buffer to display RAM
  this->set_window();
  
  this->command(0x24);  // Write RAM command
  
  // Calculate the buffer size based on dirty region
  uint32_t width_bytes = (this->x_high_ - this->x_low_ + 3) / 4;
  uint32_t buffer_width_bytes = (this->get_width() + 3) / 4;
  
  for (uint16_t y = this->y_low_; y < this->y_high_; y++) {
    uint32_t start_index = (y * buffer_width_bytes) + (this->x_low_ / 4);
    this->data_multi(this->buffer_ + start_index, width_bytes);
  }
  
  ESP_LOGV(TAG, "Wrote buffer, region: X=%u-%u, Y=%u-%u", 
           this->x_low_, this->x_high_, this->y_low_, this->y_high_);
}

void EpaperWaveshare4Color::refresh_screen(bool partial) {
  // 4-color displays only support full refresh
  this->cmd_data(0x22, {0xF7});  // Display Update Control 2
  this->command(0x20);            // Master Activation
  
  // Wait for display to refresh (4-color displays are slower)
  this->next_delay_ = 15000;  // 15 seconds for full refresh
  
  ESP_LOGD(TAG, "Display refresh initiated");
}

void EpaperWaveshare4Color::deep_sleep() {
  // Enter deep sleep mode
  this->cmd_data(0x10, {0x01});
  this->next_delay_ = 100;
  
  ESP_LOGD(TAG, "Entering deep sleep mode");
}

}  // namespace esphome::epaper_spi
