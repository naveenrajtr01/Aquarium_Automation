#pragma once
#include <Arduino.h>

// Drives the plain white LED strip through the ESP32 LEDC PWM peripheral,
// feeding the custom dimming circuit connected to WHITE_LED_PWM_PIN.
class WhiteStrip {
public:
  void begin();

  // percent is clamped to 0-100.
  void setBrightnessPercent(uint8_t percent);
  void off();

  uint8_t getBrightnessPercent() const { return currentPercent; }

private:
  uint8_t currentPercent = 0;
};
