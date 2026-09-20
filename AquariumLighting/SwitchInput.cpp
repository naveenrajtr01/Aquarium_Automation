#include "SwitchInput.h"
#include "Config.h"

void SwitchInput::begin() {
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  bool raw = (digitalRead(SWITCH_PIN) == LOW);
  debouncedClosed = raw;
  lastRawClosed = raw;
  lastChangeTime = millis();
  openedAt = debouncedClosed ? 0 : millis();
}

SwitchEvent SwitchInput::update() {
  bool raw = (digitalRead(SWITCH_PIN) == LOW);

  if (raw != lastRawClosed) {
    lastRawClosed = raw;
    lastChangeTime = millis();
  }

  if (raw != debouncedClosed && (millis() - lastChangeTime) >= SWITCH_DEBOUNCE_MS) {
    debouncedClosed = raw;
    if (debouncedClosed) {
      lastOffDurationMs = millis() - openedAt;
      return SwitchEvent::TurnedOn;
    } else {
      openedAt = millis();
      return SwitchEvent::TurnedOff;
    }
  }

  return SwitchEvent::None;
}
