#include "RgbStrip.h"

RgbStrip::RgbStrip()
  : strip(RGB_LED_COUNT, RGB_DATA_PIN, RGB_LED_TYPE) {
}

void RgbStrip::begin() {
  strip.begin();
  off();
}

void RgbStrip::setColorPercent(uint8_t redPercent, uint8_t greenPercent, uint8_t bluePercent) {
  uint8_t clampedRed = min<uint8_t>(redPercent, 100);
  uint8_t clampedGreen = min<uint8_t>(greenPercent, 100);
  uint8_t clampedBlue = min<uint8_t>(bluePercent, 100);

  // Round rather than truncate: the dashboard already loses precision
  // converting its 0-255 color picker down to a 0-100 percent value, so
  // truncating here compounds that error and made non-primary/gradient
  // colors look visibly off despite primary colors (which land on exact
  // percentages) looking fine.
  uint8_t r = (255 * clampedRed + 50) / 100;
  uint8_t g = (255 * clampedGreen + 50) / 100;
  uint8_t b = (255 * clampedBlue + 50) / 100;
  uint32_t color = strip.Color(r, g, b);

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  show();
}

void RgbStrip::off() {
  setColorPercent(0, 0, 0);
}

void RgbStrip::setPixelRGB(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(index, strip.Color(r, g, b));
}

void RgbStrip::show() {
  // WS2812's single-wire protocol is timing-sensitive, and WiFi's radio
  // interrupts run at a priority high enough to preempt it - occasionally
  // corrupting a pixel or two mid-transmission (seen as a stray stuck
  // red/blue pixel that needs a second update to clear). Re-sending the
  // identical frame immediately after is cheap (<1ms for a short strip)
  // and self-heals any single glitched transmission.
  strip.show();
  strip.show();
}

uint16_t RgbStrip::pixelCount() const {
  return strip.numPixels();
}
