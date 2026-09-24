#pragma once
#include <Arduino.h>
#include <BLECharacteristic.h>

// Mirrors important lifecycle/state log lines to the Serial monitor and,
// once attached, to a BLE notify characteristic so the phone app can see
// the same log stream without a USB connection.
class Logger {
public:
  static void begin(unsigned long serialBaud);
  static void attachBleCharacteristic(BLECharacteristic *characteristic);
  static void log(const String &message);
  static void logf(const char *format, ...);

private:
  static BLECharacteristic *bleLogChar;
};
