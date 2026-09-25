#pragma once
#include <Arduino.h>

// Mirrors important lifecycle/state log lines to the Serial monitor and,
// once attached, to a web dashboard sink so a phone can see the same log
// stream without a USB connection.
class Logger {
public:
  static void begin(unsigned long serialBaud);

  // Registers a callback invoked with every log line (in addition to
  // Serial) - used by WebDashboard to push lines to connected log-stream
  // clients.
  static void attachWebSink(void (*sink)(const String &message));

  static void log(const String &message);
  static void logf(const char *format, ...);

private:
  static void (*webSink)(const String &message);
};
