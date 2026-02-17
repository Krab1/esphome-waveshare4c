#include "epaper_213gv2.h"
#include "colorconv.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome::epaper_spi {

static const char *const TAG = "epaper_spi.213gv2";

bool EPaper213GV2::initialise(bool partial) {
  // Send base init sequence
  EPaperBase::initialise(partial);
  
  if (partial) {
    // Fast refresh initialization
    init_fast_();
  } else {
    // Full initialization
    // Set resolution - TRES command (0x61)
    this->cmd_data(0x61, {
      0x00,  // WIDTH_H
      0x7C,  // WIDTH_L (122 = 0x7C)
      0x00,  // HEIGHT_H  
      0xFA   // HEIGHT_L (250 = 0xFA)
    });
    
    // Unknown command from datasheet
    this->cmd_data(0xE9, {0x01});
    
    // Power on
    this->command(0x04);
  }
  
  return true;
}

void EPaper213GV2::init_fast_() {
  // Set resolution - TRES command (0x61)
  this->cmd_data(0x61, {
    0x00,  // WIDTH_H
    0x7C,  // WIDTH_L (122 = 0x7C)
    0x00,  // HEIGHT_H  
    0xFA   // HEIGHT_L (250 = 0xFA)
  });
  
  // Fast refresh settings
  this->cmd_data(0xE0, {0x02});
  this->cmd_data(0xE6, {90});
  
  this->command(0xA5);
  this->cmd_data(0xE9, {0x01});
  
  // Power on
  this->command(0x04);
}

bool HOT EPaper213GV2::transfer_data() {
  auto start_time = millis();
  
  if (this->current_data_index_ == 0) {
    // Start data transmission - command 0x10
    this->command(0x10);
    this->start_data_();
  }
  
  // Calculate width in bytes (4 pixels per byte)
  uint16_t width_bytes = this->row_width_;
  
  // Send image data row by row
  while (this->current_data_index_ < this->height_) {
    size_t row_start = this->current_data_index_ * width_bytes;
    
    // The display only uses the first 31 bytes per row
    // This matches the original driver behavior
    for (uint16_t x = 0; x < width_bytes && x < 31; x++) {
      this->write_byte(this->buffer_[row_start + x]);
    }
    // Pad remaining bytes with 0x00 if needed
    for (uint16_t x = width_bytes; x < 31; x++) {
      this->write_byte(0x00);
    }
    
    this->current_data_index_++;
    
    if (millis() - start_time > MAX_TRANSFER_TIME) {
      // Let the main loop run and come back next loop
      this->disable();
      return false;
    }
  }
  
  this->disable();
  this->current_data_index_ = 0;
  return true;
}

void EPaper213GV2::power_on() {
  // Power is controlled via the 0x04 command in initialise()
  // No separate power on needed
}

void EPaper213GV2::refresh_screen(bool partial) {
  // Display refresh command
  this->cmd_data(0x12, {0x00});
}

void EPaper213GV2::power_off() {
  // Power off
  this->cmd_data(0x02, {0x00});
  this->next_delay_ = 100;  // Required delay
}

void EPaper213GV2::deep_sleep() {
  // Deep sleep
  this->cmd_data(0x07, {0xA5});
}

void EPaper213GV2::fill(Color color) {
  // If clipping is active, fall back to base implementation
  if (this->get_clipping().is_set()) {
    Display::fill(color);
    return;
  }
  
  uint8_t fill_color = this->color_to_4color_(color);
  // Pack 4 pixels into one byte
  uint8_t fill_byte = (fill_color << 6) | (fill_color << 4) | (fill_color << 2) | fill_color;
  
  this->buffer_.fill(fill_byte);
  this->x_high_ = this->width_;
  this->y_high_ = this->height_;
  this->x_low_ = 0;
  this->y_low_ = 0;
}

void HOT EPaper213GV2::draw_pixel_at(int x, int y, Color color) {
  if (!rotate_coordinates_(x, y))
    return;
  
  // Calculate byte position
  uint32_t byte_pos = (y * this->row_width_) + (x / 4);
  
  // Calculate bit position within byte (2 bits per pixel)
  // Pixel 0 = bits 7-6, Pixel 1 = bits 5-4, Pixel 2 = bits 3-2, Pixel 3 = bits 1-0
  uint8_t bit_shift = 6 - ((x % 4) * 2);
  
  // Convert color to 2-bit value
  uint8_t pixel_color = this->color_to_4color_(color);
  
  // Clear the 2 bits and set new color
  this->buffer_[byte_pos] &= ~(0x03 << bit_shift);
  this->buffer_[byte_pos] |= (pixel_color << bit_shift);
}

uint8_t EPaper213GV2::color_to_4color_(Color color) {
  // Use the colorconv utility for BWYR conversion
  return color_to_bwyr(color, COLOR_BLACK, COLOR_WHITE, COLOR_YELLOW, COLOR_RED);
}

}  // namespace esphome::epaper_spi