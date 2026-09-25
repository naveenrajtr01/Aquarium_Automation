#include "StateManager.h"
#include <Preferences.h>
#include "Config.h"
#include "Presets.h"
#include "Logger.h"

namespace {
// Classic NeoPixel rainbow wheel: maps 0-255 to a color that transitions
// red -> green -> blue -> red, used to render the scheduled animation.
void wheelToRGB(uint8_t pos, uint8_t &r, uint8_t &g, uint8_t &b) {
  pos = 255 - pos;
  if (pos < 85) {
    r = 255 - pos * 3; g = 0; b = pos * 3;
  } else if (pos < 170) {
    pos -= 85;
    r = 0; g = pos * 3; b = 255 - pos * 3;
  } else {
    pos -= 170;
    r = pos * 3; g = 255 - pos * 3; b = 0;
  }
}
} // namespace

void StateManager::begin(WhiteStrip *whiteStripPtr, RgbStrip *rgbStripPtr, Buzzer *buzzerPtr) {
  whiteStrip = whiteStripPtr;
  rgbStrip = rgbStripPtr;
  buzzer = buzzerPtr;
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
  Logger::logf("Initial state: light=%s preset=%d (%s)",
               lightOn ? "ON" : "OFF", presetIndex, PRESETS[presetIndex].name);
}

void StateManager::handleSwitchTurnedOff() {
  lightOn = false;
  phase = Phase::Normal; // abandon any in-progress animation/fade-in
  pushOffToOutputs();
  Logger::log("Switch OFF -> lights off");
}

void StateManager::handleSwitchTurnedOn() {
  lightOn = true;

  if (scheduledPending) {
    scheduledPending = false;
    Logger::log("Switch ON -> running queued scheduled sequence");
    startScheduledSequence();
    return;
  }

  // Every off->on transition deliberately advances to the next preset.
  presetIndex = (presetIndex + 1) % NUM_PRESETS;
  savePresetIndexToStorage();
  if (buzzer) buzzer->beep();
  Logger::logf("Switch ON -> next preset %d (%s)", presetIndex, PRESETS[presetIndex].name);

  // Any dashboard customization made before the light was switched off is
  // discarded here - applyPresetValues() resets to the stored preset.
  applyPresetValues();
  phase = Phase::Normal;
  pushValuesToOutputs();
}

void StateManager::handleScheduledTrigger() {
  if (!lightOn) {
    // Switch is off - queue the sequence for when it's turned back on.
    scheduledPending = true;
    Logger::log("Schedule alarm fired while switch is off -> queued");
    return;
  }
  Logger::log("Schedule alarm fired -> starting sequence");
  startScheduledSequence();
}

void StateManager::startScheduledSequence() {
  if (buzzer) buzzer->beep();
  phase = Phase::Animation;
  phaseStartMs = millis();
  lastAnimationFrameMs = 0;
  whitePercent = 0;
  whiteStrip->off();
  Logger::logf("Scheduled animation started (%lus)", SCHEDULE_ANIMATION_MS / 1000UL);
}

void StateManager::update() {
  if (pendingHwApply) {
    pendingHwApply = false;
    if (lightOn) pushValuesToOutputs();
  }

  if (!lightOn || phase == Phase::Normal) return;

  unsigned long now = millis();
  unsigned long elapsed = now - phaseStartMs;

  if (phase == Phase::Animation) {
    if (elapsed >= SCHEDULE_ANIMATION_MS) {
      beginFadeIn();
    } else if (now - lastAnimationFrameMs >= SCHEDULE_ANIMATION_FRAME_MS) {
      lastAnimationFrameMs = now;
      renderAnimationFrame(elapsed);
    }
  } else if (phase == Phase::FadeIn) {
    if (elapsed >= SCHEDULE_FADEIN_MS) {
      phase = Phase::Normal;
      whitePercent = fadeTargetWhite;
      redPercent = fadeTargetRed;
      greenPercent = fadeTargetGreen;
      bluePercent = fadeTargetBlue;
      pushValuesToOutputs();
      Logger::log("Fade-in complete");
    } else {
      renderFadeIn(elapsed);
    }
  }
}

void StateManager::renderAnimationFrame(unsigned long elapsedMs) {
  uint16_t count = rgbStrip->pixelCount();
  uint8_t offset = (uint8_t)((elapsedMs / SCHEDULE_ANIMATION_ROTATE_MS) & 0xFF);
  for (uint16_t i = 0; i < count; i++) {
    uint8_t pos = (uint8_t)(((uint32_t)i * 256 / count) + offset);
    uint8_t r, g, b;
    wheelToRGB(pos, r, g, b);
    rgbStrip->setPixelRGB(i, r, g, b);
  }
  rgbStrip->show();
}

void StateManager::beginFadeIn() {
  phase = Phase::FadeIn;
  phaseStartMs = millis();

  presetIndex = SCHEDULE_DEFAULT_PRESET_INDEX;
  savePresetIndexToStorage();

  const Preset &p = PRESETS[presetIndex];
  fadeTargetWhite = p.whitePercent;
  fadeTargetRed = p.redPercent;
  fadeTargetGreen = p.greenPercent;
  fadeTargetBlue = p.bluePercent;

  whitePercent = 0;
  redPercent = 0;
  greenPercent = 0;
  bluePercent = 0;
  pushOffToOutputs();
  Logger::logf("Animation done -> fading into preset %d (%s) over 2s", presetIndex, p.name);
}

void StateManager::renderFadeIn(unsigned long elapsedMs) {
  whitePercent = (uint8_t)((uint32_t)fadeTargetWhite * elapsedMs / SCHEDULE_FADEIN_MS);
  redPercent = (uint8_t)((uint32_t)fadeTargetRed * elapsedMs / SCHEDULE_FADEIN_MS);
  greenPercent = (uint8_t)((uint32_t)fadeTargetGreen * elapsedMs / SCHEDULE_FADEIN_MS);
  bluePercent = (uint8_t)((uint32_t)fadeTargetBlue * elapsedMs / SCHEDULE_FADEIN_MS);
  pushValuesToOutputs();
}

bool StateManager::setWhiteBrightnessPercent(uint8_t percent) {
  if (!lightOn) return false;
  whitePercent = min<uint8_t>(percent, 100);
  pendingHwApply = true;
  return true;
}

bool StateManager::setRedPercent(uint8_t percent) {
  if (!lightOn) return false;
  redPercent = min<uint8_t>(percent, 100);
  pendingHwApply = true;
  return true;
}

bool StateManager::setGreenPercent(uint8_t percent) {
  if (!lightOn) return false;
  greenPercent = min<uint8_t>(percent, 100);
  pendingHwApply = true;
  return true;
}

bool StateManager::setBluePercent(uint8_t percent) {
  if (!lightOn) return false;
  bluePercent = min<uint8_t>(percent, 100);
  pendingHwApply = true;
  return true;
}

bool StateManager::setPresetIndex(uint8_t index) {
  if (!lightOn) return false;
  if (index >= NUM_PRESETS) return false;

  presetIndex = index;
  savePresetIndexToStorage();
  if (buzzer) buzzer->beep();
  applyPresetValues();
  phase = Phase::Normal;
  pendingHwApply = true;
  Logger::logf("Preset set directly to %d (%s)", presetIndex, PRESETS[presetIndex].name);
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
