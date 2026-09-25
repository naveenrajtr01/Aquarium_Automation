#pragma once
#include <Arduino.h>
#include "StateManager.h"
#include "RtcManager.h"

// Hosts a mobile-friendly web dashboard over WiFi (local network only),
// protected by HTTP Basic Auth (WEB_AUTH_USERNAME/PASSWORD in Config.h).
// Lets a phone browser control white/RGB brightness, pick a preset and the
// daily schedule time, and stream live logs on demand - all via a plain
// prebuilt browser, no app to build/install.
class WebDashboard {
public:
  void begin(StateManager *stateManager, RtcManager *rtcManager);

  // Call every loop() iteration - cheap; just prunes stale WebSocket clients.
  void update();
};
