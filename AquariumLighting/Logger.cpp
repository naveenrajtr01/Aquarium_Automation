#include "Logger.h"
#include <stdarg.h>

void (*Logger::webSink)(const String &message) = nullptr;

void Logger::begin(unsigned long serialBaud) {
  Serial.begin(serialBaud);
}

void Logger::attachWebSink(void (*sink)(const String &message)) {
  webSink = sink;
}

void Logger::log(const String &message) {
  Serial.println(message);
  if (webSink) webSink(message);
}

void Logger::logf(const char *format, ...) {
  char buf[160];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  log(String(buf));
}
