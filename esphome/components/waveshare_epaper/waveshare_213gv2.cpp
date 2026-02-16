#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2.13gv2";

// Display commands
static const uint8_t CMD_POWER_ON = 0x04;
static const uint8_t CMD_WRITE_RAM = 0x10;
static const uint8_t CMD_REFRESH = 0x12;
static const uint8_t CMD_TRES = 0x61;  // Resolution setting
static const uint8_t CMD_E9 = 0xE9;
static const uint8_t CMD_DEEP_SLEEP = 0x10;

void WaveshareEPaper2P13InGV2::setup() {
  this->init_internal_(this->get_buffer_length_());
  this->setup_pins_();
  this->spi_setup();
  
  // Initial reset
  this->reset_();
  delay(200);
  
  ESP_LOGI(TAG, "Initializing 2.13\" G V2 (122x250 controller, 4-color)");
  
  // Wait for display to be ready after reset
  delay(100);
  
  // Set resolution (TRES command) - EXACTLY as Arduino does
  this->command(CMD_TRES);
  this->data(0x00);  // Width high byte
  this->data(0x7A);  // Width low byte (122 = 0x7A)
  this->data(0x00);  // Height high byte
  this->data(0xFA);  // Height low byte (250 = 0xFA)
  
  // Additional initialization
  this->command(CMD_E9);
  this->data(0x01);
  
  // Power on
  this->command(CMD_POWER_ON);
  this->wait_until_idle_();
  
  // Clear the buffer with white to prevent random pixels
  this->fill(Color(255, 255, 255));
  
  ESP_LOGI(TAG, "Initialization complete");
}

void WaveshareEPaper2P13InGV2::initialize() {
  // Called by base class setup, but we handle initialization in setup()
}

void WaveshareEPaper2P13InGV2::display() {
  ESP_LOGI(TAG, "Updating display...");
  
  // Write buffer to display RAM
  this->command(CMD_WRITE_RAM);
  
  // EXACTLY as Arduino: 31 bytes per line, 250 lines
  const uint16_t width_bytes = 31;  // 122 / 4 = 30.5, round up to 31
  const uint16_t height = 250;
  
  this->start_data_();
  
  // Send buffer data line by line, EXACTLY as Arduino does
  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < width_bytes; x++) {
      uint16_t idx = y * width_bytes + x;
      if (idx < this->get_buffer_length_()) {
        this->write_byte(this->buffer_[idx]);
      } else {
        this->write_byte(0xFF);  // White fill
      }
    }
  }
  
  this->end_data_();
  
  // Trigger refresh - EXACTLY as Arduino
  this->command(CMD_REFRESH);
  this->data(0x00);
  
  // Wait for display
  this->wait_until_idle_();
  
  ESP_LOGI(TAG, "Display update complete");
}

void WaveshareEPaper2P13InGV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13in G V2 (4-color)");
  ESP_LOGCONFIG(TAG, "  Controller Resolution: 122x250");
  ESP_LOGCONFIG(TAG, "  Colors: Black, White, Red, Yellow");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  // Enter deep sleep mode
  this->command(CMD_DEEP_SLEEP);
  this->data(0x01);
}

int WaveshareEPaper2P13InGV2::get_width_internal() { 
  return 122;  // Controller width (even if physical display is rotated)
}

int WaveshareEPaper2P13InGV2::get_height_internal() { 
  return 250;  // Controller height (even if physical display is rotated)
}

uint32_t WaveshareEPaper2P13InGV2::get_buffer_length_() {
  // 4 pixels per byte (2 bits per pixel)
  // 122 pixels / 4 = 30.5, round up to 31 bytes per line
  // MUST match Arduino: 31 bytes × 250 lines = 7,750 bytes
  return 31 * 250;
}

uint32_t WaveshareEPaper2P13InGV2::idle_timeout_() { 
  return 10000;  // 10 seconds timeout
}

// Color mapping for 4-color display:
// 0x0 = Black
// 0x1 = White  
// 0x2 = Yellow
// 0x3 = Red
uint8_t WaveshareEPaper2P13InGV2::color_to_4color(Color color) {
  // Check if color matches special colors (red/yellow)
  // Red: high red, low green/blue
  if (color.r > 200 && color.g < 100 && color.b < 100) {
    return 0x3;  // Red
  }
  // Yellow: high red and green, low blue
  if (color.r > 200 && color.g > 200 && color.b < 100) {
    return 0x2;  // Yellow
  }
  // Orange/Brown (treat as yellow)
  if (color.r > 150 && color.g > 100 && color.g < 200 && color.b < 100) {
    return 0x2;  // Yellow
  }
  
  // Black/White based on brightness
  uint8_t brightness = (color.r + color.g + color.b) / 3;
  if (brightness > 127) {
    return 0x1;  // White
  } else {
    return 0x0;  // Black
  }
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t pixel_value = this->color_to_4color(color);
  // Pack 4 pixels per byte
  uint8_t fill_byte = (pixel_value << 6) | (pixel_value << 4) | (pixel_value << 2) | pixel_value;
  memset(this->buffer_, fill_byte, this->get_buffer_length_());
}

void WaveshareEPaper2P13InGV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0) {
    return;
  }
  
  // EXACTLY as Arduino buffer layout: 31 bytes per line
  const uint16_t width_bytes = 31;  // 122 / 4 = 30.5, round up to 31
  uint16_t byte_index = y * width_bytes + (x / 4);
  uint8_t bit_offset = (3 - (x % 4)) * 2;  // 2 bits per pixel
  
  if (byte_index >= this->get_buffer_length_()) {
    return;
  }
  
  uint8_t pixel_value = this->color_to_4color(color);
  
  // Clear the 2 bits for this pixel and set new value
  this->buffer_[byte_index] &= ~(0x3 << bit_offset);
  this->buffer_[byte_index] |= (pixel_value << bit_offset);
}

}  // namespace waveshare_epaper
}  // namespace esphome