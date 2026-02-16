#include "waveshare_epaper.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2.13gv2";

// Display resolution - CORRECTED based on user feedback
static const uint16_t DISPLAY_WIDTH = 250;   // Horizontal pixels
static const uint16_t DISPLAY_HEIGHT = 122;  // Vertical pixels
static const uint8_t BYTES_PER_LINE = 63;    // 250 / 4 = 62.5, rounded up to 63

// Display commands
static const uint8_t CMD_POWER_ON = 0x04;
static const uint8_t CMD_WRITE_RAM = 0x10;
static const uint8_t CMD_REFRESH = 0x12;
static const uint8_t CMD_TRES = 0x61;
static const uint8_t CMD_E9 = 0xE9;
static const uint8_t CMD_E0 = 0xE0;
static const uint8_t CMD_E6 = 0xE6;
static const uint8_t CMD_A5 = 0xA5;
static const uint8_t CMD_POWER_OFF = 0x02;
static const uint8_t CMD_DEEP_SLEEP = 0x07;

// Color codes for 4-color display
static const uint8_t COLOR_BLACK = 0x0;
static const uint8_t COLOR_WHITE = 0x1;
static const uint8_t COLOR_YELLOW = 0x2;
static const uint8_t COLOR_RED = 0x3;

void WaveshareEPaper2P13InGV2::setup() {
  // Initialize buffer
  this->init_internal_(this->get_buffer_length_());
  this->setup_pins_();
  this->spi_setup();
  
  // Hardware reset
  this->reset_();
  delay(200);
  
  ESP_LOGI(TAG, "Initializing 2.13\" G V2 (250x122, 4-color, landscape)");
  
  delay(100);
  
  // Send resolution (TRES command 0x61)
  // CORRECTED: Width=250, Height=122
  this->command(CMD_TRES);
  this->data(0x00);  // Width high byte
  this->data(0xFA);  // Width low byte (250 decimal = 0xFA hex)
  this->data(0x00);  // Height high byte
  this->data(0x7A);  // Height low byte (122 decimal = 0x7A hex)
  
  // Optional: Fast mode
  if (this->fast_mode_) {
    ESP_LOGI(TAG, "Enabling fast mode");
    this->command(CMD_E0);
    this->data(0x02);
    
    this->command(CMD_E6);
    this->data(90);
    
    this->command(CMD_A5);
    this->wait_until_idle_();
  }
  
  // Enable E9 register
  this->command(CMD_E9);
  this->data(0x01);
  
  // Power on
  this->command(CMD_POWER_ON);
  this->wait_until_idle_();
  
  // Clear buffer
  this->fill(Color(255, 255, 255));
  
  ESP_LOGI(TAG, "Initialization complete (mode: %s)", 
           this->fast_mode_ ? "fast" : "normal");
}

void WaveshareEPaper2P13InGV2::initialize() {
  // Called by base class, handled in setup()
}

void WaveshareEPaper2P13InGV2::display() {
  ESP_LOGI(TAG, "Updating display...");
  
  this->command(CMD_WRITE_RAM);
  this->start_data_();
  
  // Send buffer data
  // 63 bytes per line × 122 lines
  const uint16_t height = this->get_height_internal();
  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < BYTES_PER_LINE; x++) {
      uint16_t idx = y * BYTES_PER_LINE + x;
      if (idx < this->get_buffer_length_()) {
        this->write_byte(this->buffer_[idx]);
      } else {
        this->write_byte(0xFF);  // White fill
      }
    }
  }
  
  this->end_data_();
  
  // Trigger refresh
  this->command(CMD_REFRESH);
  this->data(0x00);
  this->wait_until_idle_();
  
  ESP_LOGI(TAG, "Display update complete");
}

void WaveshareEPaper2P13InGV2::dump_config() {
  LOG_DISPLAY("", "Waveshare E-Paper", this);
  ESP_LOGCONFIG(TAG, "  Model: 2.13in G V2 (4-color)");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d (landscape)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
  ESP_LOGCONFIG(TAG, "  Colors: Black, White, Yellow, Red");
  ESP_LOGCONFIG(TAG, "  Fast Mode: %s", this->fast_mode_ ? "YES" : "NO");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_UPDATE_INTERVAL(this);
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  ESP_LOGI(TAG, "Entering deep sleep");
  
  this->command(CMD_POWER_OFF);
  this->data(0x00);
  this->wait_until_idle_();
  
  delay(100);
  
  this->command(CMD_DEEP_SLEEP);
  this->data(0xA5);
}

int WaveshareEPaper2P13InGV2::get_width_internal() { 
  return DISPLAY_WIDTH;  // 250 pixels
}

int WaveshareEPaper2P13InGV2::get_height_internal() { 
  return DISPLAY_HEIGHT;  // 122 pixels
}

uint32_t WaveshareEPaper2P13InGV2::get_buffer_length_() {
  // CORRECTED: 63 bytes per line × 122 lines = 7,686 bytes
  return BYTES_PER_LINE * DISPLAY_HEIGHT;
}

uint32_t WaveshareEPaper2P13InGV2::idle_timeout_() { 
  return 10000;
}

uint8_t WaveshareEPaper2P13InGV2::color_to_4color(Color color) {
  // Red: high red, low green/blue
  if (color.r > 200 && color.g < 100 && color.b < 100) {
    return COLOR_RED;
  }
  
  // Yellow: high red and green, low blue
  if (color.r > 200 && color.g > 200 && color.b < 100) {
    return COLOR_YELLOW;
  }
  
  // Orange/Brown → Yellow
  if (color.r > 150 && color.g > 100 && color.g < 200 && color.b < 100) {
    return COLOR_YELLOW;
  }
  
  // Black/White based on brightness
  uint8_t brightness = (color.r + color.g + color.b) / 3;
  if (brightness > 127) {
    return COLOR_WHITE;
  } else {
    return COLOR_BLACK;
  }
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t pixel_value = this->color_to_4color(color);
  uint8_t fill_byte = (pixel_value << 6) | (pixel_value << 4) | 
                      (pixel_value << 2) | pixel_value;
  memset(this->buffer_, fill_byte, this->get_buffer_length_());
}

void WaveshareEPaper2P13InGV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  // Bounds checking - CORRECTED for 250×122
  if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
    return;
  }
  
  // Calculate byte position
  // CORRECTED: 63 bytes per line (not 31)
  uint16_t byte_index = y * BYTES_PER_LINE + (x / 4);
  uint8_t bit_offset = (3 - (x % 4)) * 2;
  
  if (byte_index >= this->get_buffer_length_()) {
    return;
  }
  
  uint8_t pixel_value = this->color_to_4color(color);
  
  // Clear and set bits
  this->buffer_[byte_index] &= ~(0x3 << bit_offset);
  this->buffer_[byte_index] |= (pixel_value << bit_offset);
}

void WaveshareEPaper2P13InGV2::set_fast_mode(bool fast_mode) {
  this->fast_mode_ = fast_mode;
}

}  // namespace waveshare_epaper
}  // namespace esphome