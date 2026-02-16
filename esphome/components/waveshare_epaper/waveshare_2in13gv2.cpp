#include "waveshare_2in13gv2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2.13g_v2";

// Display resolution
static const uint8_t EPD_2IN13G_V2_WIDTH = 122;
static const uint8_t EPD_2IN13G_V2_HEIGHT = 250;

// Color definitions
static const uint8_t COLOR_BLACK = 0x0;
static const uint8_t COLOR_WHITE = 0x1;
static const uint8_t COLOR_YELLOW = 0x2;
static const uint8_t COLOR_RED = 0x3;

void WaveshareEPaper2P13InGV2::initialize() {
  this->init_internal_7c_(this->get_buffer_length_());
  this->setup_pins_();
  
  delay(100);
  this->init_display_();
}

void WaveshareEPaper2P13InGV2::init_display_() {
  // Hardware reset
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(200);
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    delay(200);
  }
  
  this->wait_until_idle_();
  
  // Set resolution (TRES command 0x61)
  this->command(0x61);
  this->data(0x00);  // Width high byte
  this->data(0x7C);  // Width low byte (124)
  this->data(0x00);  // Height high byte
  this->data(0xFA);  // Height low byte (250)
  
  if (this->fast_mode_) {
    // Fast mode initialization
    this->command(0xE0);
    this->data(0x02);
    
    this->command(0xE6);
    this->data(90);
    
    this->command(0xA5);
    this->wait_until_idle_();
  }
  
  // Enable E9 register
  this->command(0xE9);
  this->data(0x01);
  
  // Power on
  this->command(0x04);
  this->wait_until_idle_();
  
  ESP_LOGI(TAG, "Display initialized in %s mode", this->fast_mode_ ? "fast" : "normal");
}

void WaveshareEPaper2P13InGV2::display() {
  // Send display data
  uint32_t buf_len = this->get_buffer_length_();
  
  // Command 0x10 - Write data to display
  this->command(0x10);
  
  // Calculate byte width (4 pixels per byte due to 2-bit color depth)
  uint32_t width_bytes = (EPD_2IN13G_V2_WIDTH % 4 == 0) ? 
                         (EPD_2IN13G_V2_WIDTH / 4) : 
                         (EPD_2IN13G_V2_WIDTH / 4 + 1);
  
  // Send buffer data
  for (uint16_t y = 0; y < EPD_2IN13G_V2_HEIGHT; y++) {
    for (uint16_t x_byte = 0; x_byte < width_bytes; x_byte++) {
      // Only send valid display area (first 31 bytes per line)
      if (x_byte < 31) {
        uint32_t idx = x_byte + y * width_bytes;
        if (idx < buf_len) {
          this->data(this->buffer_[idx]);
        } else {
          this->data(0x00);
        }
      } else {
        this->data(0x00);
      }
    }
  }
  
  this->turn_on_display_();
}

void WaveshareEPaper2P13InGV2::turn_on_display_() {
  // Refresh display (command 0x12)
  this->command(0x12);
  this->data(0x00);
  
  // Wait for display to finish updating
  this->wait_until_idle_();
}

void WaveshareEPaper2P13InGV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13in G V2 (4-color)");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", EPD_2IN13G_V2_WIDTH, EPD_2IN13G_V2_HEIGHT);
  ESP_LOGCONFIG(TAG, "  Fast Mode: %s", this->fast_mode_ ? "YES" : "NO");
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  // Power off command
  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();
  
  delay(100);  // Minimum 100ms delay required
  
  // Deep sleep command
  this->command(0x07);
  this->data(0xA5);  // Check byte
}

int WaveshareEPaper2P13InGV2::get_width_internal() {
  return EPD_2IN13G_V2_WIDTH;
}

int WaveshareEPaper2P13InGV2::get_height_internal() {
  return EPD_2IN13G_V2_HEIGHT;
}

uint8_t WaveshareEPaper2P13InGV2::get_color_code(Color color) {
  // Convert ESPHome Color to display color code
  // The display uses 2 bits per pixel for 4 colors
  
  uint8_t red = color.red;
  uint8_t green = color.green;
  uint8_t blue = color.blue;
  
  // Calculate brightness
  uint16_t brightness = red + green + blue;
  
  // Map to 4 colors based on color characteristics
  // Black: low brightness
  if (brightness < 150) {
    return COLOR_BLACK;
  }
  
  // Yellow: high red and green, low blue
  if (red > 200 && green > 200 && blue < 100) {
    return COLOR_YELLOW;
  }
  
  // Red: high red, low green and blue
  if (red > 200 && green < 100 && blue < 100) {
    return COLOR_RED;
  }
  
  // Default to white for everything else
  return COLOR_WHITE;
}

}  // namespace waveshare_epaper
}  // namespace esphome
