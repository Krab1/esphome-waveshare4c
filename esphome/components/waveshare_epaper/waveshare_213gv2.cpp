#include "waveshare_213gv2.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace waveshare_epaper {

static const char *const TAG = "waveshare_2p13_g_v2";

static const uint16_t EPD_WIDTH = 122;
static const uint16_t EPD_HEIGHT = 250;

static const uint8_t COLOR_BLACK = 0x0;
static const uint8_t COLOR_WHITE = 0x1;
static const uint8_t COLOR_YELLOW = 0x2;
static const uint8_t COLOR_RED = 0x3;

// ---------------------------------------------------------------------------
// Actual ESPHome call flow (read waveshare_epaper.cpp before touching this):
//
//   WaveshareEPaperBase::setup()   [called ONCE at boot]
//     → init_internal_()           allocates pixel buffer
//     → setup_pins_()
//     → reset_()                   hardware reset (base class)
//     → initialize()               our function – called exactly once
//
//   WaveshareEPaperBase::update()  [called every update_interval]
//     → do_update_()               runs the display lambda / pages
//     → display()                  our function – called every interval
//
// Root causes of the boot blink:
//   1. initialize() called reset_() again → DOUBLE reset on boot.
//   2. initialize() called init_internal_() again → double buffer allocation.
//
// Root cause of the periodic blink:
//   3. display() called init_full_() (which calls reset_()) every
//      full_update_every_ cycles → unnecessary hardware reset mid-operation.
//
// Fix:
//   • initialize() does ZERO hardware-reset calls; the base already reset.
//   • initialize() does ZERO init_internal_() calls; the base already allocated.
//   • initialize() just configures registers once.
//   • display() sends pixel data and triggers refresh only.  Full vs. fast
//     waveform is selected by register 0xE0, not by resetting the panel.
// ---------------------------------------------------------------------------

void WaveshareEPaper2P13InGV2::initialize() {
  // Base class setup() already called reset_() and init_internal_() before us.
  // Just configure the panel registers; no reset, no buffer allocation here.

  // Set resolution – TRES (0x61)
  this->command(0x61);
  this->data(0x00);  // WIDTH_H
  this->data(0x7C);  // WIDTH_L  (122 = 0x7C)
  this->data(0x00);  // HEIGHT_H
  this->data(0xFA);  // HEIGHT_L (250 = 0xFA)

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
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);

  bool full_refresh = (this->at_update_ == 0);
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  if (full_refresh) {
    // Full waveform mode: switch register then power on.
    // No hardware reset needed – just select the waveform via 0xE0.
    this->command(0xE0);
    this->data(0x00);  // 0x00 = full waveform, 0x02 = fast waveform
    this->command(0x04);
    this->wait_until_idle_();
  } else {
    // Fast refresh mode
    this->command(0xE0);
    this->data(0x02);

    this->command(0xE6);
    this->data(90);

    this->command(0xA5);
    this->wait_until_idle_();
  }

  this->command(0xE9);
  this->data(0x01);

  // Send pixel data
  this->command(0x10);
  for (uint16_t y = 0; y < EPD_HEIGHT; y++) {
    for (uint16_t x = 0; x < width_bytes; x++) {
      if (x < 31) {
        this->data(this->buffer_[x + y * width_bytes]);
      } else {
        this->data(0x00);
      }
    }
  }

  // Trigger panel refresh
  this->command(0x12);
  this->data(0x00);
  this->wait_until_idle_();
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  this->at_update_ = 0;

  // Power off
  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();
  delay(100);  // NOLINT

  // Deep sleep
  this->command(0x07);
  this->data(0xA5);
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t fill_color = this->color_to_4color_(color);
  uint8_t fill_byte = (fill_color << 6) | (fill_color << 4) | (fill_color << 2) | fill_color;
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
  uint8_t red = color.r;
  uint8_t green = color.g;
  uint8_t blue = color.b;
  uint16_t brightness = red + green + blue;

  if (brightness < 80)
    return COLOR_BLACK;
  if (red > 200 && red > green * 1.5f && red > blue * 1.5f)
    return COLOR_RED;
  if (red > 200 && green > 200 && blue < 100)
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