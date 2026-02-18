#include "waveshare_213gv2.h"
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

// ---------------------------------------------------------------------------
// initialize() is called by the ESPHome framework before every display()
// call.  The original code reset the hardware here unconditionally, which
// caused the panel to run its full ~15-second blink sequence on every single
// update cycle.
//
// Fix: split the one-time hardware bring-up (init_full_) from the lightweight
// per-update preparation (init_fast_).  Use the initialized_ flag so that
// the slow full init only ever runs once after power-on or deep-sleep wake.
// Subsequent calls to initialize() pick the fast path, which only takes a
// few hundred milliseconds and does not cause visible blinking.
// ---------------------------------------------------------------------------

void WaveshareEPaper2P13InGV2::initialize() {
  if (!this->initialized_) {
    // First call: allocate buffer and do the full hardware init.
    this->init_internal_(this->get_buffer_length_());
    this->init_full_();
    this->initialized_ = true;
  } else {
    // Every subsequent call: lightweight re-init for fast refresh.
    // Do NOT reset at_update_ here; that counter is managed in display().
    this->init_fast_();
  }
}

// Full power-on initialisation (runs once).
void WaveshareEPaper2P13InGV2::init_full_() {
  this->reset_();
  this->wait_until_idle_();

  // Set resolution - TRES command (0x61)
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

// Fast/partial re-init (runs before every subsequent refresh).
// No reset pulse → no blinking.
void WaveshareEPaper2P13InGV2::init_fast_() {
  // Set resolution
  this->command(0x61);
  this->data(0x00);
  this->data(0x7C);
  this->data(0x00);
  this->data(0xFA);

  // Fast refresh mode
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
  // Width in bytes: each byte holds 4 pixels (2 bits per pixel).
  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);

  // Decide whether this cycle is a full or fast refresh BEFORE sending data.
  // Previously this decision was made inside initialize(), but at_update_ was
  // then modified here, meaning the two functions saw different counter values.
  bool do_full = (this->at_update_ == 0);

  // Advance counter for next cycle.
  this->at_update_ = (this->at_update_ + 1) % this->full_update_every_;

  if (do_full) {
    // Full refresh: re-run the slow hardware init so the panel applies its
    // full waveform sequence.  This is expected to blink, but only every
    // full_update_every_ cycles rather than every single update.
    this->init_full_();
  }
  // Fast refresh: init_fast_() was already called by initialize() above.

  // Start data transmission
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

  this->turn_on_display_();
}

void WaveshareEPaper2P13InGV2::turn_on_display_() {
  this->command(0x12);
  this->data(0x00);
  this->wait_until_idle_();
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  // Reset initialized_ so the next wake-up runs init_full_ again.
  this->initialized_ = false;

  this->command(0x02);
  this->data(0x00);
  this->wait_until_idle_();
  delay(100);  // NOLINT

  this->command(0x07);
  this->data(0xA5);
}

void WaveshareEPaper2P13InGV2::fill(Color color) {
  uint8_t fill_color = this->color_to_4color_(color);
  uint8_t fill_byte = (fill_color << 6) | (fill_color << 4) | (fill_color << 2) | fill_color;
  uint32_t buffer_length = this->get_buffer_length_();
  for (uint32_t i = 0; i < buffer_length; i++) {
    this->buffer_[i] = fill_byte;
  }
}

void HOT WaveshareEPaper2P13InGV2::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x >= this->get_width_internal() || y >= this->get_height_internal() || x < 0 || y < 0)
    return;

  uint16_t width_bytes = (EPD_WIDTH % 4 == 0) ? (EPD_WIDTH / 4) : (EPD_WIDTH / 4 + 1);
  uint32_t byte_pos = (y * width_bytes) + (x / 4);
  // Pixel 0 → bits 7-6, pixel 1 → bits 5-4, pixel 2 → bits 3-2, pixel 3 → bits 1-0
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