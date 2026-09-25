#include "RtcManager.h"
#include <Wire.h>
#include <Preferences.h>
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

  loadScheduleFromStorage();
  armAlarm();
}

void RtcManager::update() {
  // No SQW/INT pin is wired, so the alarm flag is polled over I2C instead of
  // via a hardware interrupt.
  if (rtc.alarmFired(1)) {
    rtc.clearAlarm(1);
    alarmPending = true;
    Logger::logf("RTC alarm fired at %s", rtc.now().timestamp().c_str());
  }

  // Only place that touches the RTC/Wire bus besides begin()/setEpoch()/
  // armAlarm() - keeps all I2C access on this one task. See getTimeString()/
  // isTimeValid() comments in the header for why.
  char buf[24];
  DateTime now = rtc.now();
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());
  cachedTimeString = buf;
  cachedTimeValid = !rtc.lostPower();
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

void RtcManager::setScheduleTime(uint8_t hour, uint8_t minute) {
  if (hour > 23 || minute > 59) return;
  scheduleHour = hour;
  scheduleMinute = minute;
  saveScheduleToStorage();
  armAlarm();
}

void RtcManager::loadScheduleFromStorage() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true); // read-only
  scheduleHour = prefs.getUChar(PREFS_KEY_SCHEDULE_HOUR, SCHEDULE_HOUR);
  scheduleMinute = prefs.getUChar(PREFS_KEY_SCHEDULE_MINUTE, SCHEDULE_MINUTE);
  prefs.end();
  if (scheduleHour > 23) scheduleHour = SCHEDULE_HOUR;
  if (scheduleMinute > 59) scheduleMinute = SCHEDULE_MINUTE;
}

void RtcManager::saveScheduleToStorage() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false); // read-write
  prefs.putUChar(PREFS_KEY_SCHEDULE_HOUR, scheduleHour);
  prefs.putUChar(PREFS_KEY_SCHEDULE_MINUTE, scheduleMinute);
  prefs.end();
}

void RtcManager::armAlarm() {
  // DS3231_A1_Hour matches hour/minute/second only (ignores date), so this
  // alarm re-fires every day at scheduleHour:scheduleMinute:SCHEDULE_SECOND
  // without needing to be reprogrammed.
  rtc.setAlarm1(
    DateTime(2000, 1, 1, scheduleHour, scheduleMinute, SCHEDULE_SECOND),
    DS3231_A1_Hour);
  Logger::logf("RTC daily alarm armed for %02d:%02d:%02d", scheduleHour, scheduleMinute, SCHEDULE_SECOND);
}
