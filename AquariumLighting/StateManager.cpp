#include "StateManager.h"
#include <Preferences.h>
#include "Config.h"
#include "Presets.h"

void StateManager::begin(WhiteStrip *whiteStripPtr, RgbStrip *rgbStripPtr) {
  whiteStrip = whiteStripPtr;
  rgbStrip = rgbStripPtr;
  loadPresetIndexFromStorage();
}

void StateManager::applyInitialState(bool switchClosedAtBoot) {
  applyPresetValues();
  if (switchClosedAtBoot) {
    lightOn = true;
    pushValuesToOutputs();
  } else {
    lightOn = false;
    pushOffToOutputs();
  }
}

void StateManager::handleSwitchTurnedOff() {
  lightOn = false;
  pushOffToOutputs();
}

void StateManager::handleSwitchTurnedOn(unsigned long offDurationMs) {
  if (offDurationMs < QUICK_TOGGLE_THRESHOLD_MS) {
    presetIndex = (presetIndex + 1) % NUM_PRESETS;
    savePresetIndexToStorage();
  }
  // Any BLE customization made before the light was switched off is
  // discarded here - applyPresetValues() resets to the stored preset.
  applyPresetValues();
  lightOn = true;
  pushValuesToOutputs();
}

bool StateManager::setWhiteBrightnessPercent(uint8_t percent) {
  if (!lightOn) return false;
  whitePercent = min<uint8_t>(percent, 100);
  whiteStrip->setBrightnessPercent(whitePercent);
  return true;
}

bool StateManager::setRedPercent(uint8_t percent) {
  if (!lightOn) return false;
  redPercent = min<uint8_t>(percent, 100);
  rgbStrip->setColorPercent(redPercent, greenPercent, bluePercent);
  return true;
}

bool StateManager::setGreenPercent(uint8_t percent) {
  if (!lightOn) return false;
  greenPercent = min<uint8_t>(percent, 100);
  rgbStrip->setColorPercent(redPercent, greenPercent, bluePercent);
  return true;
}

bool StateManager::setBluePercent(uint8_t percent) {
  if (!lightOn) return false;
  bluePercent = min<uint8_t>(percent, 100);
  rgbStrip->setColorPercent(redPercent, greenPercent, bluePercent);
  return true;
}

void StateManager::loadPresetIndexFromStorage() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, true); // read-only
  presetIndex = prefs.getUChar(PREFS_KEY_PRESET_INDEX, 0);
  prefs.end();
  if (presetIndex >= NUM_PRESETS) presetIndex = 0;
}

void StateManager::savePresetIndexToStorage() {
  Preferences prefs;
  prefs.begin(PREFS_NAMESPACE, false); // read-write
  prefs.putUChar(PREFS_KEY_PRESET_INDEX, presetIndex);
  prefs.end();
}

void StateManager::applyPresetValues() {
  const Preset &p = PRESETS[presetIndex];
  whitePercent = p.whitePercent;
  redPercent = p.redPercent;
  greenPercent = p.greenPercent;
  bluePercent = p.bluePercent;
}

void StateManager::pushValuesToOutputs() {
  whiteStrip->setBrightnessPercent(whitePercent);
  rgbStrip->setColorPercent(redPercent, greenPercent, bluePercent);
}

void StateManager::pushOffToOutputs() {
  whiteStrip->off();
  rgbStrip->off();
}
