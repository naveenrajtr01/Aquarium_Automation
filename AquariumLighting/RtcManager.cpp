#include "RtcManager.h"
#include <Wire.h>
#include "Config.h"
#include "Logger.h"

void RtcManager::begin() {
  Wire.begin(RTC_SDA_PIN, RTC_SCL_PIN);
  rtc.begin();

  if (rtc.lostPower()) {
    // First-ever boot (or battery removed): seed with compile time so the
    // clock isn't wildly wrong until the app sends the accurate time.
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    Logger::log("RTC lost power - seeded from compile time (set accurate time via BLE)");
  }
  Logger::logf("RTC current time: %s", rtc.now().timestamp().c_str());

  // DS3231_A1_Hour matches hour/minute/second only (ignores date), so this
  // alarm re-fires every day at SCHEDULE_HOUR:SCHEDULE_MINUTE:SCHEDULE_SECOND
  // without needing to be reprogrammed.
  rtc.setAlarm1(
    DateTime(2000, 1, 1, SCHEDULE_HOUR, SCHEDULE_MINUTE, SCHEDULE_SECOND),
    DS3231_A1_Hour);
  Logger::logf("RTC daily alarm armed for %02d:%02d:%02d", SCHEDULE_HOUR, SCHEDULE_MINUTE, SCHEDULE_SECOND);
}

void RtcManager::update() {
  // No SQW/INT pin is wired, so the alarm flag is polled over I2C instead of
  // via a hardware interrupt.
  if (rtc.alarmFired(1)) {
    rtc.clearAlarm(1);
    alarmPending = true;
    Logger::logf("RTC alarm fired at %s", rtc.now().timestamp().c_str());
  }
}

bool RtcManager::consumeScheduledTrigger() {
  if (!alarmPending) return false;
  alarmPending = false;
  return true;
}

void RtcManager::setEpoch(uint32_t epochSeconds) {
  rtc.adjust(DateTime(epochSeconds));
  Logger::logf("RTC time set to %s", rtc.now().timestamp().c_str());
}
