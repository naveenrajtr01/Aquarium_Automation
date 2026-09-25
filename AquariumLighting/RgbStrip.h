#pragma once
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Config.h"

// Drives the addressable RGB strip. Adafruit_NeoPixel's global setBrightness()
// rescales stored colors and loses precision, so instead each channel's 0-100%
// value is converted straight into an 8-bit color component and written to
// every pixel - color and "brightness" are simply the RGB values themselves.
class RgbStrip {
public:
  RgbStrip();

  void begin();

  // Each parameter is a 0-100% intensity for that color channel.
  void setColorPercent(uint8_t redPercent, uint8_t greenPercent, uint8_t bluePercent);
  void off();

  // Raw per-pixel control (0-255 per channel) used by the schedule
  // animation. Bypasses the tracked percent values - call setColorPercent()
  // afterwards to return to normal uniform-color operation.
  void setPixelRGB(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
  void show();
  uint16_t pixelCount() const;

  // Live-tunable per-channel correction (see RGB_CHANNEL_*_SCALE in
  // Config.h) - lets the dashboard's calibration sliders re-tint the
  // strip without a reflash, since the right values vary per strip.
  void setWhiteBalance(uint8_t rScale, uint8_t gScale, uint8_t bScale);
  void getWhiteBalance(uint8_t &rScale, uint8_t &gScale, uint8_t &bScale) const;

private:
  // Scales each channel by its current white-balance factor.
  void applyWhiteBalance(uint8_t &r, uint8_t &g, uint8_t &b) const;

  Adafruit_NeoPixel strip;
  uint8_t wbRedScale = RGB_CHANNEL_R_SCALE;
  uint8_t wbGreenScale = RGB_CHANNEL_G_SCALE;
  uint8_t wbBlueScale = RGB_CHANNEL_B_SCALE;
};
