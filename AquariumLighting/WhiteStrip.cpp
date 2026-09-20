#include "WhiteStrip.h"
#include "Config.h"

void WhiteStrip::begin() {
  ledcSetup(WHITE_PWM_CHANNEL, WHITE_PWM_FREQ_HZ, WHITE_PWM_RESOLUTION_BITS);
  ledcAttachPin(WHITE_LED_PWM_PIN, WHITE_PWM_CHANNEL);
  off();
}

void WhiteStrip::setBrightnessPercent(uint8_t percent) {
  currentPercent = min<uint8_t>(percent, 100);
  uint32_t maxDuty = (1UL << WHITE_PWM_RESOLUTION_BITS) - 1;
  uint32_t duty = (maxDuty * currentPercent) / 100;
  ledcWrite(WHITE_PWM_CHANNEL, duty);
}

void WhiteStrip::off() {
  setBrightnessPercent(0);
}
