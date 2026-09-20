#pragma once
#include <Arduino.h>
#include "WhiteStrip.h"
#include "RgbStrip.h"

// Owns the light's overall behavior: which preset is active, on/off state,
// persistence of the last-used preset, and applying/overriding values on the
// physical outputs. This is the only class that decides what the strips show.
class StateManager {
public:
  void begin(WhiteStrip *whiteStrip, RgbStrip *rgbStrip);

  // Call once from setup(), after begin(), with the switch's debounced state
  // at boot. Boot is always treated like a "long time off" - i.e. the last
  // stored preset is restored, never advanced.
  void applyInitialState(bool switchClosedAtBoot);

  // Switch transition handlers (call from loop() when SwitchInput reports them).
  void handleSwitchTurnedOff();
  void handleSwitchTurnedOn(unsigned long offDurationMs);

  // BLE write requests. Return true if applied, false if rejected (light off).
  // On rejection the tracked/reported value is left unchanged.
  bool setWhiteBrightnessPercent(uint8_t percent);
  bool setRedPercent(uint8_t percent);
  bool setGreenPercent(uint8_t percent);
  bool setBluePercent(uint8_t percent);

  // BLE read accessors - always reflect what the strips currently show (or,
  // while off, what they will show again once switched back on).
  bool isOn() const { return lightOn; }
  uint8_t getPresetIndex() const { return presetIndex; }
  uint8_t getWhitePercent() const { return whitePercent; }
  uint8_t getRedPercent() const { return redPercent; }
  uint8_t getGreenPercent() const { return greenPercent; }
  uint8_t getBluePercent() const { return bluePercent; }

private:
  void loadPresetIndexFromStorage();
  void savePresetIndexToStorage();
  void applyPresetValues();   // load presetIndex's values into whitePercent/r/g/b
  void pushValuesToOutputs(); // write whitePercent/r/g/b to the physical strips
  void pushOffToOutputs();

  WhiteStrip *whiteStrip = nullptr;
  RgbStrip *rgbStrip = nullptr;

  bool lightOn = false;
  uint8_t presetIndex = 0;

  // Currently active values, either from the preset or overridden via BLE.
  uint8_t whitePercent = 0;
  uint8_t redPercent = 0;
  uint8_t greenPercent = 0;
  uint8_t bluePercent = 0;
};
