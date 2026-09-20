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

  uint8_t getRedPercent() const { return currentRedPercent; }
  uint8_t getGreenPercent() const { return currentGreenPercent; }
  uint8_t getBluePercent() const { return currentBluePercent; }

private:
  Adafruit_NeoPixel strip;
  uint8_t currentRedPercent = 0;
  uint8_t currentGreenPercent = 0;
  uint8_t currentBluePercent = 0;
};
