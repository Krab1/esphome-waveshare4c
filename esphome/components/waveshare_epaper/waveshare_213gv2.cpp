#include "waveshare_213gv2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2p13_g_v2";

static const uint16_t EPD_WIDTH = 122;
static const uint16_t EPD_HEIGHT = 250;

static const uint8_t COLOR_BLACK  = 0x0;
static const uint8_t COLOR_WHITE  = 0x1;
static const uint8_t COLOR_YELLOW = 0x2;
static const uint8_t COLOR_RED    = 0x3;

void WaveshareEPaper2P13InGV2::initialize() {
  ESP_LOGD(TAG, "initialize(): first_call_=%d, at_update_=%d", (int) this->first_call_, this->at_update_);

  if (this->first_call_) {
    this->first_call_ = false;
    ESP_LOGD(TAG, "initialize(): skipping reset (base already reset at boot)");
  } else {
    if (this->reset_pin_ != nullptr) {
      ESP_LOGD(TAG, "initialize(): performing hardware reset");
      this->reset_pin_->digital_write(true);
      delay(200);  // NOLINT
      this->reset_pin_->digital_write(false);
      delay(2);
      this->reset_pin_->digital_write(true);
      delay(200);  // NOLINT
    }
  }

  this->wait_until_idle_();
  ESP_LOGD(TAG, "initialize(): busy cleared");

  if (this->at_update_ == 0) {
    ESP_LOGD(TAG, "initialize(): FULL waveform init");
    // Exact original sequence that was confirmed working
    this->command(0x61);
    this->data(0x00);
    this->data(0x7C);  // WIDTH 122
    this->data(0x00);
    this->data(0xFA);  // HEIGHT 250
    this->command(0xE9);
    this->data(0x01);
    this->command(0x04);  // power on
    this->wait_until_idle_();
    ESP_LOGD(TAG, "initialize(): full init done");

  } else {
    ESP_LOGD(TAG, "initialize(): FAST waveform init");
    // Same structure as full, but with fast waveform registers inserted.
    // Key difference from original init_fast_(): 0xA5 is removed from here
    // and the sequence mirrors full path so the data transmission (0x10 + 0x12)
    // works the same way.
    this->command(0x61);
    this->data(0x00);
    this->data(0x7C);
    this->data(0x00);
    this->data(0xFA);
    this->command(0xE0);
    this->data(0x02);  // fast waveform mode
    this->command(0xE6);
    this->data(90);    // fast refresh time
    this->command(0xE9);
    this->data(0x01);
    this->command(0x04);  // power on
    this->wait_until_idle_();
    ESP_LOGD(TAG, "initialize(): fast init done");
  }
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
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);

  this->at_update_++;
  if (this->at_update_ >= this->full_update_every_) {
    this->at_update_ = 0;
  }

  ESP_LOGD(TAG, "display(): at_update_=%d (full_update_every_=%d), full=%s",
           this->at_update_, this->full_update_every_,
           this->at_update_ == 0 ? "YES" : "no");

  this->initialize();

  ESP_LOGD(TAG, "display(): sending %d rows x %d bytes", EPD_HEIGHT, width_bytes);
  this->command(0x10);
  for (uint16_t y = 0; y < EPD_HEIGHT; y++) {
    for (uint16_t x = 0; x < width_bytes; x++) {
      this->data((x < 31) ? this->buffer_[x + y * width_bytes] : 0x00);
    }
  }
  ESP_LOGD(TAG, "display(): pixel data sent");

  this->turn_on_display_();
  ESP_LOGD(TAG, "display(): done");
}

void WaveshareEPaper2P13InGV2::turn_on_display_() {
  ESP_LOGD(TAG, "turn_on_display_(): sending 0x12, waiting...");
  this->command(0x12);
  this->data(0x00);
  uint32_t start = millis();
  this->wait_until_idle_();
  ESP_LOGD(TAG, "turn_on_display_(): done in %d ms", (int)(millis() - start));
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  ESP_LOGD(TAG, "deep_sleep()");
  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();
  delay(100);  // NOLINT
  this->command(0x07);
  this->data(0xA5);
  this->first_call_ = true;
  this->at_update_ = 0;
  ESP_LOGD(TAG, "deep_sleep(): done");
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t fill_color = this->color_to_4color_(color);
  uint8_t fill_byte = (fill_color << 6) | (fill_color << 4) | (fill_color << 2) | fill_color;
  ESP_LOGD(TAG, "fill(): color r=%d g=%d b=%d -> 4color=%d byte=0x%02X",
           color.r, color.g, color.b, fill_color, fill_byte);
  for (uint32_t i = 0; i < this->get_buffer_length_(); i++) {
    this->buffer_[i] = fill_byte;
  }
}

void HOT WaveshareEPaper2P13InGV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  uint32_t byte_pos = (y * width_bytes) + (x / 4);
  uint8_t bit_shift = 6 - ((x % 4) * 2);
  uint8_t pixel_color = this->color_to_4color_(color);
  this->buffer_[byte_pos] &= ~(0x03 << bit_shift);
  this->buffer_[byte_pos] |= (pixel_color << bit_shift);
}

uint8_t WaveshareEPaper2P13InGV2::color_to_4color_(Color color) {
  uint16_t brightness = color.r + color.g + color.b;
  if (brightness < 80)
    return COLOR_BLACK;
  if (color.r > 200 && color.r > color.g * 1.5f && color.r > color.b * 1.5f)
    return COLOR_RED;
  if (color.r > 200 && color.g > 200 && color.b < 100)
    return COLOR_YELLOW;
  return COLOR_WHITE;
}

uint32_t WaveshareEPaper2P13InGV2::get_buffer_length_() {
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  return width_bytes * EPD_HEIGHT;
}

int WaveshareEPaper2P13InGV2::get_width_internal() { return EPD_WIDTH; }
int WaveshareEPaper2P13InGV2::get_height_internal() { return EPD_HEIGHT; }

}  // namespace waveshare_epaper
}  // namespace esphome