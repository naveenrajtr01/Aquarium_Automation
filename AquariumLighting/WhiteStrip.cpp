#include "WhiteStrip.h"
#include "Config.h"

void WhiteStrip::begin() {
  ledcAttach(
      WHITE_LED_PWM_PIN,
      WHITE_PWM_FREQ_HZ,
      WHITE_PWM_RESOLUTION_BITS
  );

  off();
}

void WhiteStrip::setBrightnessPercent(uint8_t percent) {
  currentPercent = min<uint8_t>(percent, 100);

  uint32_t maxDuty =
      (1UL << WHITE_PWM_RESOLUTION_BITS) - 1;

  uint32_t duty =
      (maxDuty * currentPercent) / 100;

  // The dimming circuit inverts the PWM signal (0 duty = physically full
  // bright, max duty = physically off), so invert here to keep the percent
  // parameter meaning what it says (0 = off, 100 = full brightness).
  ledcWrite(
      WHITE_LED_PWM_PIN,
      maxDuty - duty
  );
}

void WhiteStrip::off() {
  setBrightnessPercent(0);
}
