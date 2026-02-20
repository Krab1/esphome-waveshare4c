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

// ---------------------------------------------------------------------------
// Call flow (confirmed from waveshare_epaper.cpp):
//
//   WaveshareEPaperBase::setup()          [ONCE at boot]
//     → init_internal_(get_buffer_length_())   ← buffer allocated HERE
//     → setup_pins_()
//     → reset_()                               ← hardware reset HERE
//     → initialize()                           ← our function, ONCE
//
//   WaveshareEPaperBase::update()         [every update_interval]
//     → do_update_()   runs lambda/pages
//     → display()      ← our function, every cycle
//
// What was wrong before:
//   1. initialize() called reset_() again → double reset = long blink on boot.
//   2. initialize() called init_internal_() again → double buffer allocation.
//   3. init_fast_() ALSO called reset_() → blink on every fast refresh.
//   4. display() never set the fast/full waveform mode because init_fast_()
//      was only called from initialize(), which is only called once at boot
//      (always with at_update_==0 → full path). Fast mode was NEVER used.
//
// Pattern used (same as GDEW029T5 in waveshare_epaper.cpp):
//   initialize() → minimal one-time register setup, no reset, no buffer alloc
//   display()    → selects full or fast mode at top, sends data, triggers refresh
// ---------------------------------------------------------------------------

// Shared helper: write resolution registers
void WaveshareEPaper2P13InGV2::set_resolution_() {
  this->command(0x61);
  this->data(0x00);  // WIDTH_H
  this->data(0x7C);  // WIDTH_L  (122)
  this->data(0x00);  // HEIGHT_H
  this->data(0xFA);  // HEIGHT_L (250)
}

// Shared helper: power on and wait
void WaveshareEPaper2P13InGV2::power_on_() {
  this->command(0x04);
  this->wait_until_idle_();
}

// Full waveform init — resets the panel.
// Called inside display() for periodic full refreshes (every full_update_every_
// cycles after the first). Intentional blink, same as any 4-color e-paper.
void WaveshareEPaper2P13InGV2::init_full_() {
  // base class reset_() polarity: low→high. Our panel needs high→low→high.
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->digital_write(true);
    delay(200);  // NOLINT
    this->reset_pin_->digital_write(false);
    delay(2);
    this->reset_pin_->digital_write(true);
    delay(200);  // NOLINT
  }
  this->wait_until_idle_();
  this->set_resolution_();
  this->command(0xE9);
  this->data(0x01);
  this->power_on_();
}

// Fast waveform init — NO reset, NO blink.
// Called inside display() for every normal (non-full) refresh.
void WaveshareEPaper2P13InGV2::init_fast_() {
  // No reset here. Original code had reset_() in this function — that was the
  // cause of blinking on every fast refresh cycle.
  this->set_resolution_();
  this->command(0xE0);
  this->data(0x02);  // fast waveform mode
  this->command(0xE6);
  this->data(90);
  this->command(0xA5);
  this->wait_until_idle_();
  this->command(0xE9);
  this->data(0x01);
  this->power_on_();
}

// Called ONCE by base class setup(), after reset_() and init_internal_().
// Just set up registers for the initial display — no reset, no buffer alloc.
void WaveshareEPaper2P13InGV2::initialize() {
  this->set_resolution_();
  this->command(0xE9);
  this->data(0x01);
  this->power_on_();
  // at_update_ stays 0 so first display() treats it as a full refresh.
  // But first_display_ flag prevents an unnecessary reset right after boot.
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
    if (this->first_display_) {
      // Very first display() after boot: initialize() already set up the panel
      // correctly and base class already reset it. Just proceed to send data.
      this->first_display_ = false;
    } else {
      // Periodic full refresh: reset + full waveform. Will blink — expected.
      this->init_full_();
    }
  } else {
    // Fast refresh: mode registers only, no reset, no blink.
    this->init_fast_();
  }

  // Send pixel data
  this->command(0x10);
  for (uint16_t y = 0; y < EPD_HEIGHT; y++) {
    for (uint16_t x = 0; x < width_bytes; x++) {
      this->data((x < 31) ? this->buffer_[x + y * width_bytes] : 0x00);
    }
  }

  // Trigger panel refresh
  this->command(0x12);
  this->data(0x00);
  this->wait_until_idle_();
}

void WaveshareEPaper2P13InGV2::deep_sleep() {
  this->at_update_ = 0;
  this->first_display_ = true;  // next wake-up should skip reset on first display

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