#pragma once
#include <Arduino.h>
#include <BLECharacteristic.h>

// Mirrors important lifecycle/state log lines to the Serial monitor and,
// once attached, to a BLE notify characteristic and/or a web dashboard sink
// so a phone can see the same log stream without a USB connection.
class Logger {
public:
  static void begin(unsigned long serialBaud);
  static void attachBleCharacteristic(BLECharacteristic *characteristic);

  // Registers a callback invoked with every log line (in addition to Serial/
  // BLE) - used by WebDashboard to push lines to connected log-stream clients.
  static void attachWebSink(void (*sink)(const String &message));

  static void log(const String &message);
  static void logf(const char *format, ...);

private:
  static BLECharacteristic *bleLogChar;
  static void (*webSink)(const String &message);
};
