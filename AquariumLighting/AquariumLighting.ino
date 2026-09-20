// ---------------------------------------------------------------------------
// Aquarium Lighting Controller
//
// White LED strip (PWM dimmed) + addressable RGB strip, controlled by a
// physical on/off + preset-cycling switch and a mobile app over Bluetooth
// Low Energy. See README.md in the repository root for wiring, behavior and
// BLE protocol details.
//
// Required libraries (install via Arduino IDE Library Manager):
//   - Adafruit NeoPixel
// Bundled with the ESP32 Arduino core (no install needed):
//   - Preferences.h, BLEDevice.h and friends
// ---------------------------------------------------------------------------

#include "Config.h"
#include "WhiteStrip.h"
#include "RgbStrip.h"
#include "SwitchInput.h"
#include "StateManager.h"
#include "BleController.h"

WhiteStrip whiteStrip;
RgbStrip rgbStrip;
SwitchInput switchInput;
StateManager stateManager;
BleController bleController;

void setup() {
  Serial.begin(115200);

  whiteStrip.begin();
  rgbStrip.begin();
  switchInput.begin();
  stateManager.begin(&whiteStrip, &rgbStrip);

  // Boot is always treated as a "long time off" - restore the last preset,
  // never advance it, regardless of which position the switch is in.
  stateManager.applyInitialState(switchInput.isClosed());

  bleController.begin(&stateManager);
}

void loop() {
  SwitchEvent event = switchInput.update();

  if (event == SwitchEvent::TurnedOff) {
    stateManager.handleSwitchTurnedOff();
    bleController.refreshAll();
  } else if (event == SwitchEvent::TurnedOn) {
    stateManager.handleSwitchTurnedOn(switchInput.getLastOffDurationMs());
    bleController.refreshAll();
  }
}
