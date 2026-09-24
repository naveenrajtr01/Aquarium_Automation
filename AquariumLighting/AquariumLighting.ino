// ---------------------------------------------------------------------------
// Aquarium Lighting Controller
//
// White LED strip (PWM dimmed) + addressable RGB strip, controlled by a
// physical on/off + preset-cycling switch, a mobile app over Bluetooth Low
// Energy, and a DS3231 real-time clock driving a daily evening schedule.
// See README.md in the repository root for wiring, behavior and BLE
// protocol details.
//
// Required libraries (install via Arduino IDE Library Manager):
//   - Adafruit NeoPixel
//   - RTClib (by Adafruit)
// Bundled with the ESP32 Arduino core (no install needed):
//   - Preferences.h, Wire.h, BLEDevice.h and friends
// ---------------------------------------------------------------------------

#include "Config.h"
#include "WhiteStrip.h"
#include "RgbStrip.h"
#include "SwitchInput.h"
#include "Buzzer.h"
#include "RtcManager.h"
#include "StateManager.h"
#include "BleController.h"
#include "Logger.h"

WhiteStrip whiteStrip;
RgbStrip rgbStrip;
SwitchInput switchInput;
Buzzer buzzer;
RtcManager rtcManager;
StateManager stateManager;
BleController bleController;

void setup() {
  Logger::begin(115200);
  Logger::log("Booting Aquarium Lighting controller...");

  whiteStrip.begin();
  Logger::log("White strip ready (GPIO25 PWM)");

  rgbStrip.begin();
  Logger::logf("RGB strip ready (%d pixels on GPIO27)", RGB_LED_COUNT);

  switchInput.begin();
  Logger::logf("Switch ready (GPIO19, currently %s)", switchInput.isClosed() ? "CLOSED/ON" : "OPEN/OFF");

  buzzer.begin();
  Logger::log("Buzzer ready (GPIO32)");

  rtcManager.begin();

  stateManager.begin(&whiteStrip, &rgbStrip, &buzzer);

  // Boot is always treated as a "long time off" - restore the last preset,
  // never advance it, regardless of which position the switch is in.
  stateManager.applyInitialState(switchInput.isClosed());

  bleController.begin(&stateManager, &rtcManager);
  Logger::log("Setup complete");
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

  buzzer.update();

  rtcManager.update();
  if (rtcManager.consumeScheduledTrigger()) {
    stateManager.handleScheduledTrigger();
    bleController.refreshAll();
  }

  stateManager.update();
}

