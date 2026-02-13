#include "epaper_waveshare_4color.h"
#include <algorithm>
#include "esphome/core/log.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.waveshare_4color";

// Color encoding for 4-color display (2 bits per pixel):
// Black  = 00 (0)
// White  = 01 (1)
// Yellow = 10 (2) - NOTE: Waveshare has Yellow/Red swapped from what I had before!
// Red    = 11 (3)

uint8_t EpaperWaveshare4Color::get_color_bits_(Color color) {
  uint8_t r = color.r;
  uint8_t g = color.g;
  uint8_t b = color.b;

  // Check for white (all high)
  if (r > 200 && g > 200 && b > 200) {
    return 0b01;  // White
  }

  // Check for yellow (high red, high green, low blue)
  if (r > 200 && g > 200 && b < 100) {
    return 0b10;  // Yellow
  }

  // Check for red (high red, low green, low blue)
  if (r > 200 && g < 100 && b < 100) {
    return 0b11;  // Red
  }

  // Default to black
  return 0b00;  // Black
}

void HOT EpaperWaveshare4Color::draw_pixel_at(int x, int y, Color color) {
  // Use base class coordinate rotation
  if (!this->rotate_coordinates_(x, y))
    return;

  // Calculate buffer position for 4-color (2 bits per pixel, 4 pixels per byte)
  const size_t pixel_index = y * this->width_ + x;
  const size_t byte_index = pixel_index / 4;  // 4 pixels per byte
  const uint8_t bit_position = (pixel_index % 4) * 2;  // 2 bits per pixel

  // Get the 2-bit color value
  const uint8_t color_bits = this->get_color_bits_(color);

  // Read current byte, clear the 2 bits for this pixel, set new value
  const uint8_t original = this->buffer_[byte_index];
  this->buffer_[byte_index] = (original & ~(0b11 << bit_position)) | (color_bits << bit_position);
}

bool EpaperWaveshare4Color::initialise(bool partial) {
  // Call parent initialization to send init sequence
  EPaperBase::initialise(partial);

  // 4-color displays don't support partial refresh
  if (partial) {
    ESP_LOGW(TAG, "Partial refresh not supported on 4-color displays, using full refresh");
  }

  // Send LUT if provided (though 4-color displays may not use traditional LUTs)
  if (this->lut_length_ > 0) {
    this->cmd_data(0x32, this->lut_, this->lut_length_);
  }

  this->send_red_ = false;  // We don't use separate red buffer for 4-color
  return true;
}

void EpaperWaveshare4Color::set_window() {
  // For 4-color displays, align to 4-pixel boundaries (4 pixels per byte)
  this->x_low_ &= ~3;  // Round down to multiple of 4
  this->x_high_ += 3;
  this->x_high_ &= ~3;  // Round up to multiple of 4
  
  // Don't allow x_high to exceed actual display width
  if (this->x_high_ > this->width_) {
    this->x_high_ = this->width_;
  }

  uint16_t x_start = this->x_low_ / 4;  // 4 pixels per byte
  uint16_t x_end = (this->x_high_ - 1) / 4;

  // Set RAM X address (horizontal)
  this->cmd_data(0x44, {(uint8_t) x_start, (uint8_t) x_end});
  this->cmd_data(0x4E, {(uint8_t) x_start});

  // Set RAM Y address (vertical)
  this->cmd_data(0x45, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256),
                        (uint8_t) (this->y_high_ - 1), (uint8_t) ((this->y_high_ - 1) / 256)});
  this->cmd_data(0x4F, {(uint8_t) this->y_low_, (uint8_t) (this->y_low_ / 256)});

  ESP_LOGV(TAG, "Set window X: %u-%u, Y: %u-%u", this->x_low_, this->x_high_, this->y_low_, this->y_high_);
}

void EpaperWaveshare4Color::refresh_screen(bool partial) {
  // 4-color displays only support full refresh
  // CRITICAL FIX: Use Waveshare's refresh command
  this->cmd_data(0x12, {0x00});  // Display refresh: 0x12 with data 0x00
  this->next_delay_ = 15000;      // 15 seconds for 4-color displays
  ESP_LOGD(TAG, "Display refresh initiated (4-color, ~15s)");
}

bool HOT EpaperWaveshare4Color::transfer_data() {
  auto start_time = millis();

  if (this->current_data_index_ == 0) {
    // Set window for the dirty region
    this->set_window();

    // CRITICAL FIX: Use Waveshare's data write command
    this->command(0x10);  // Write RAM - 0x10, not 0x24!
    this->current_data_index_ = this->y_low_;  // Track current line
  }

  size_t row_length = (this->x_high_ - this->x_low_) / 4;  // 4 pixels per byte
  FixedVector<uint8_t> bytes_to_send{};
  bytes_to_send.init(row_length);

  ESP_LOGV(TAG, "Writing %u bytes at line %zu", row_length, this->current_data_index_);

  this->start_data_();

  while (this->current_data_index_ != this->y_high_) {
    // Calculate starting position in buffer
    size_t data_idx = this->current_data_index_ * this->row_width_4color_ + this->x_low_ / 4;

    // Copy row data
    for (size_t i = 0; i != row_length; i++) {
      bytes_to_send[i] = this->buffer_[data_idx + i];
    }

    ++this->current_data_index_;
    this->write_array(&bytes_to_send.front(), row_length);  // NOLINT

    // Yield to main loop if taking too long
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      this->disable();
      return false;  // Continue next loop
    }
  }

  this->disable();
  this->current_data_index_ = 0;
  return true;  // Transfer complete
}

}  // namespace esphome::epaper_spi