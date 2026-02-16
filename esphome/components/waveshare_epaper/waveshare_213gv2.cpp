#include "waveshare_epaper_2p13_g_v2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2p13_g_v2";

// Display resolution
static const uint16_t EPD_WIDTH = 122;
static const uint16_t EPD_HEIGHT = 250;

// Color definitions (2 bits per pixel)
static const uint8_t COLOR_BLACK = 0x0;
static const uint8_t COLOR_WHITE = 0x1;
static const uint8_t COLOR_YELLOW = 0x2;
static const uint8_t COLOR_RED = 0x3;

void WaveshareEPaper2P13InGV2::initialize() {
  ESP_LOGI(TAG, "Initializing display...");
  
  this->init_internal_(this->get_buffer_length_());
  
  // Do initial reset and full initialization
  ESP_LOGD(TAG, "Resetting display");
  this->reset_();
  
  ESP_LOGD(TAG, "Waiting for display ready");
  this->wait_until_idle_();
  
  // Set resolution - TRES command (0x61)
  ESP_LOGD(TAG, "Setting resolution: %dx%d", EPD_WIDTH, EPD_HEIGHT);
  this->command(0x61);
  this->data(0x00);  // WIDTH_H
  this->data(0x7C);  // WIDTH_L (122 = 0x7C)
  this->data(0x00);  // HEIGHT_H  
  this->data(0xFA);  // HEIGHT_L (250 = 0xFA)
  
  this->command(0xE9);
  this->data(0x01);
  
  // Power on
  ESP_LOGD(TAG, "Powering on display");
  this->command(0x04);
  this->wait_until_idle_();
  
  // Clear the display once
  ESP_LOGD(TAG, "Initial clear");
  this->command(0x10);
  for (uint16_t i = 0; i < 31 * EPD_HEIGHT; i++) {
    this->data(0x55);  // White fill pattern (0x55 = 01010101 = white,white,white,white)
  }
  this->command(0x12);  // Refresh
  this->data(0x00);
  this->wait_until_idle_();
  
  ESP_LOGI(TAG, "Display initialization complete");
}

void WaveshareEPaper2P13InGV2::init_fast_() {
  this->reset_();
  this->wait_until_idle_();
  
  // Set resolution - TRES command (0x61)
  this->command(0x61);
  this->data(0x00);  // WIDTH_H
  this->data(0x7C);  // WIDTH_L (122 = 0x7C)
  this->data(0x00);  // HEIGHT_H  
  this->data(0xFA);  // HEIGHT_L (250 = 0xFA)
  
  // Fast refresh settings
  this->command(0xE0);
  this->data(0x02);
  
  this->command(0xE6);
  this->data(90);
  
  this->command(0xA5);
  this->wait_until_idle_();
  
  this->command(0xE9);
  this->data(0x01);
  
  // Power on
  this->command(0x04);
  this->wait_until_idle_();
}

void WaveshareEPaper2P13InGV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13in 4-Color (G) V2");
  ESP_LOGCONFIG(TAG, "  Width: %d", EPD_WIDTH);
  ESP_LOGCONFIG(TAG, "  Height: %d", EPD_HEIGHT);
  ESP_LOGCONFIG(TAG, "  Full Update Every: %d", this->full_update_every_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void HOT WaveshareEPaper2P13InGV2::display() {
  ESP_LOGD(TAG, "Starting display update #%d", this->at_update_ + 1);
  
  // Calculate buffer dimensions
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  
  // Track update count
  this->at_update_++;
  
  // Check if we need to switch to fast mode (only do this ONCE after first update)
  if (this->at_update_ == 1 && this->full_update_every_ > 1) {
    ESP_LOGI(TAG, "Switching to fast refresh mode");
    this->init_fast_();
  }
  
  // Reset counter for periodic full updates
  if (this->at_update_ >= this->full_update_every_) {
    this->at_update_ = 0;
    // Could add full update logic here if needed, but for now just reset counter
  }
  
  // Start data transmission - command 0x10
  this->command(0x10);
  
  // Send image data
  for (uint16_t y = 0; y < EPD_HEIGHT; y++) {
    for (uint16_t x = 0; x < width_bytes; x++) {
      if (x < 31) {
        uint8_t byte_data = this->buffer_[x + y * width_bytes];
        this->data(byte_data);
      } else {
        this->data(0x00);
      }
    }
  }
  
  ESP_LOGD(TAG, "Data sent, refreshing display");
  
  // Refresh display
  this->turn_on_display_();
  
  ESP_LOGD(TAG, "Display update complete");
}

void WaveshareEPaper2P13InGV2::turn_on_display_() {
  ESP_LOGD(TAG, "Sending display refresh command");
  
  // Display refresh command
  this->command(0x12);
  this->data(0x00);
  
  ESP_LOGD(TAG, "Waiting for refresh to complete...");
  this->wait_until_idle_();
  
  ESP_LOGD(TAG, "Display refresh complete");
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  // Power off
  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();
  delay(100);  // NOLINT - required delay
  
  // Deep sleep
  this->command(0x07);
  this->data(0xA5);
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t fill_color = this->color_to_4color_(color);
  // Pack 4 pixels into one byte
  uint8_t fill_byte = (fill_color << 6) | (fill_color << 4) | (fill_color << 2) | fill_color;
  
  uint32_t buffer_length = this->get_buffer_length_();
  for (uint32_t i = 0; i < buffer_length; i++) {
    this->buffer_[i] = fill_byte;
  }
}

void HOT WaveshareEPaper2P13InGV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0) {
    return;
  }
  
  // Calculate buffer dimensions
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  
  // Calculate byte position
  uint32_t byte_pos = (y * width_bytes) + (x / 4);
  
  // Calculate bit position within byte (2 bits per pixel)
  // Pixel 0 = bits 7-6, Pixel 1 = bits 5-4, Pixel 2 = bits 3-2, Pixel 3 = bits 1-0
  uint8_t bit_shift = 6 - ((x % 4) * 2);
  
  // Convert color to 2-bit value
  uint8_t pixel_color = this->color_to_4color_(color);
  
  // Clear the 2 bits and set new color
  this->buffer_[byte_pos] &= ~(0x03 << bit_shift);
  this->buffer_[byte_pos] |= (pixel_color << bit_shift);
}

uint8_t WaveshareEPaper2P13InGV2::color_to_4color_(Color color) {
  // Convert RGB color to 4-color e-paper colors
  // Priority: Black > Red > Yellow > White
  
  uint8_t red = color.r;
  uint8_t green = color.g;
  uint8_t blue = color.b;
  
  // Calculate brightness
  uint16_t brightness = red + green + blue;
  
  // Black - if very dark
  if (brightness < 80) {
    return COLOR_BLACK;
  }
  
  // Red - if red dominant and not too yellow
  if (red > 200 && red > green * 1.5 && red > blue * 1.5) {
    return COLOR_RED;
  }
  
  // Yellow - if red and green are high
  if (red > 200 && green > 200 && blue < 100) {
    return COLOR_YELLOW;
  }
  
  // Default to white for bright colors
  return COLOR_WHITE;
}

uint32_t WaveshareEPaper2P13InGV2::get_buffer_length_() {
  // 4 pixels per byte (2 bits per pixel)
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  return width_bytes * EPD_HEIGHT;
}

int WaveshareEPaper2P13InGV2::get_width_internal() {
  return EPD_WIDTH;
}

int WaveshareEPaper2P13InGV2::get_height_internal() {
  return EPD_HEIGHT;
}

}  // namespace waveshare_epaper
}  // namespace esphome