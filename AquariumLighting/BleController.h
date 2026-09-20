#pragma once
#include <Arduino.h>
#include "StateManager.h"

// Wraps the ESP32 BLE GATT server: one service exposing on/off state,
// active preset, and white/red/green/blue percentages. Writable
// characteristics forward requests to StateManager; all characteristics are
// refreshed/notified via refreshAll() whenever state changes.
class BleController {
public:
  void begin(StateManager *stateManager);

  // Push the current StateManager values into every characteristic and
  // notify connected clients. Call after any switch-triggered state change.
  void refreshAll();
};
