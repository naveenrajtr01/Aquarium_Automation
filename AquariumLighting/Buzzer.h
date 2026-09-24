#pragma once
#include <Arduino.h>

// Non-blocking short beep used as feedback on mode changes (manual preset
// cycling and the automatic scheduled lighting change). Assumes an
// active-high buzzer module; swap the digitalWrite levels in Buzzer.cpp if
// yours is active-low.
class Buzzer {
public:
  void begin();

  // Starts (or restarts) a BUZZER_BEEP_MS beep. Safe to call repeatedly.
  void beep();

  // Call every loop() iteration - turns the buzzer off once the beep
  // duration has elapsed.
  void update();

private:
  bool sounding = false;
  unsigned long startedAtMs = 0;
};
