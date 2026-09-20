#include "RgbStrip.h"

RgbStrip::RgbStrip()
  : strip(RGB_LED_COUNT, RGB_DATA_PIN, RGB_LED_TYPE) {
}

void RgbStrip::begin() {
  strip.begin();
  off();
}

void RgbStrip::setColorPercent(uint8_t redPercent, uint8_t greenPercent, uint8_t bluePercent) {
  currentRedPercent = min<uint8_t>(redPercent, 100);
  currentGreenPercent = min<uint8_t>(greenPercent, 100);
  currentBluePercent = min<uint8_t>(bluePercent, 100);

  uint8_t r = (255 * currentRedPercent) / 100;
  uint8_t g = (255 * currentGreenPercent) / 100;
  uint8_t b = (255 * currentBluePercent) / 100;
  uint32_t color = strip.Color(r, g, b);

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

void RgbStrip::off() {
  setColorPercent(0, 0, 0);
}
