#pragma once
#include <Arduino.h>

enum class SwitchEvent {
  None,       // no state change since last update()
  TurnedOn,   // switch just closed (debounced)
  TurnedOff   // switch just opened (debounced)
};

// Debounced reader for the on/off toggle switch (wired pin-to-GND, internal
// pull-up enabled, so a closed switch reads LOW).
class SwitchInput {
public:
  void begin();

  // Call every loop() iteration. Returns TurnedOn/TurnedOff on a debounced
  // state change, otherwise None.
  SwitchEvent update();

  // Current debounced state.
  bool isClosed() const { return debouncedClosed; }

  // Valid right after a TurnedOn event: how long the switch was open before
  // closing again.
  unsigned long getLastOffDurationMs() const { return lastOffDurationMs; }

private:
  bool debouncedClosed = false;
  bool lastRawClosed = false;
  unsigned long lastChangeTime = 0;
  unsigned long openedAt = 0;
  unsigned long lastOffDurationMs = 0;
};
