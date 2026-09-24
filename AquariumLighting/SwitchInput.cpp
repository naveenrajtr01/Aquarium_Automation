#include "SwitchInput.h"
#include "Config.h"

void SwitchInput::begin() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Wait for a stable reading before trusting the switch position at boot -
  // a single instant read can catch a power-on transient and report the
  // wrong initial mode.
  lastRawClosed = (digitalRead(SWITCH_PIN) == LOW);
  lastChangeTime = millis();
  while (millis() - lastChangeTime < SWITCH_DEBOUNCE_MS) {
    bool raw = (digitalRead(SWITCH_PIN) == LOW);
    if (raw != lastRawClosed) {
      lastRawClosed = raw;
      lastChangeTime = millis();
    }
  }

  debouncedClosed = lastRawClosed;
  lastChangeTime = millis();
}

SwitchEvent SwitchInput::update() {
  bool raw = (digitalRead(SWITCH_PIN) == LOW);

  if (raw != lastRawClosed) {
    lastRawClosed = raw;
    lastChangeTime = millis();
  }

  if (raw != debouncedClosed && (millis() - lastChangeTime) >= SWITCH_DEBOUNCE_MS) {
    debouncedClosed = raw;
    return debouncedClosed ? SwitchEvent::TurnedOn : SwitchEvent::TurnedOff;
  }

  return SwitchEvent::None;
}
