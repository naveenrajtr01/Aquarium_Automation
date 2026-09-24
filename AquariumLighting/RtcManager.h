#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include "Config.h"

// Wraps the DS3231 real-time clock: keeps time across power cycles/resets
// and fires a recurring daily alarm used to trigger the evening lighting
// schedule.
//
// Time accuracy: the delay between compiling and the sketch actually
// starting to run on the ESP32 (upload + boot time) means a compile-time
// (__DATE__/__TIME__) set is already stale by the time it's applied, so
// that is only ever used as a first-boot fallback (RTC never set / lost
// battery backup). For accurate time, write the current Unix epoch seconds
// (e.g. from your phone's NTP-synced clock) to the "Set Time" BLE
// characteristic - see README.md.
class RtcManager {
public:
  void begin();

  // Call every loop() iteration.
  void update();

  // Returns true exactly once when the daily schedule alarm has fired since
  // the last call.
  bool consumeScheduledTrigger();

  // Sets the RTC to the given Unix epoch time. Whatever timezone your app
  // sends is what the schedule alarm hour/minute/second are compared in.
  void setEpoch(uint32_t epochSeconds);

  // Runtime-adjustable daily schedule, persisted in NVS (falls back to
  // Config.h's SCHEDULE_HOUR/SCHEDULE_MINUTE on first boot). Re-arms the
  // DS3231 alarm immediately.
  void setScheduleTime(uint8_t hour, uint8_t minute);
  uint8_t getScheduleHour() const { return scheduleHour; }
  uint8_t getScheduleMinute() const { return scheduleMinute; }

  // Current time as "YYYY-MM-DD HH:MM:SS". isTimeValid() is false if the
  // RTC has never been set accurately (still on the compile-time fallback).
  String getTimeString();
  bool isTimeValid();

private:
  void loadScheduleFromStorage();
  void saveScheduleToStorage();
  void armAlarm();

  RTC_DS3231 rtc;
  bool alarmPending = false;
  uint8_t scheduleHour = SCHEDULE_HOUR;
  uint8_t scheduleMinute = SCHEDULE_MINUTE;
};
