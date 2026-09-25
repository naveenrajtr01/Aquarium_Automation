#pragma once
#include <Arduino.h>
#include "WhiteStrip.h"
#include "RgbStrip.h"
#include "Buzzer.h"

// Owns the light's overall behavior: which preset is active, on/off state,
// persistence of the last-used preset, and applying/overriding values on the
// physical outputs. This is the only class that decides what the strips show.
class StateManager {
public:
  void begin(WhiteStrip *whiteStrip, RgbStrip *rgbStrip, Buzzer *buzzer);

  // Call once from setup(), after begin(), with the switch's debounced state
  // at boot. Boot is always treated like a "long time off" - i.e. the last
  // stored preset is restored, never advanced.
  void applyInitialState(bool switchClosedAtBoot);

  // Switch transition handlers (call from loop() when SwitchInput reports them).
  void handleSwitchTurnedOff();
  void handleSwitchTurnedOn();

  // Call from loop() whenever RtcManager reports the daily schedule alarm
  // fired. If the switch is off, the sequence is queued and runs as soon as
  // the switch is turned back on.
  void handleScheduledTrigger();

  // Call every loop() iteration - advances any in-progress scheduled
  // animation/fade-in, and applies any pending write from setWhite/Red/
  // Green/BluePercent()/setPresetIndex(). No-op the rest of the time.
  void update();

  // Write requests from the web dashboard (runs on the AsyncWebServer
  // callback task, not the main loop() task). These only update the
  // tracked percent values and flag a pending hardware write - the actual
  // whiteStrip/rgbStrip I/O always happens from update() on the main task,
  // to avoid two tasks touching the NeoPixel driver concurrently (which
  // caused briefly-wrong colors when a preset was selected). Return true if
  // applied, false if rejected (light off).
  bool setWhiteBrightnessPercent(uint8_t percent);
  bool setRedPercent(uint8_t percent);
  bool setGreenPercent(uint8_t percent);
  bool setBluePercent(uint8_t percent);

  // Directly selects a preset (used by the web dashboard's preset buttons).
  // Only applies while the light is on; returns false (no-op) otherwise.
  bool setPresetIndex(uint8_t index);

  // Always reflect what the strips currently show (or, while off, what
  // they will show again once switched back on).
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

  void startScheduledSequence();               // begins the animation phase
  void renderAnimationFrame(unsigned long elapsedMs);
  void beginFadeIn();                          // switches from animation to fade-in phase
  void renderFadeIn(unsigned long elapsedMs);

  WhiteStrip *whiteStrip = nullptr;
  RgbStrip *rgbStrip = nullptr;
  Buzzer *buzzer = nullptr;

  bool lightOn = false;
  uint8_t presetIndex = 0;

  // Currently active values, either from the preset or overridden via the web dashboard.
  uint8_t whitePercent = 0;
  uint8_t redPercent = 0;
  uint8_t greenPercent = 0;
  uint8_t bluePercent = 0;

  // Scheduled evening sequence: rainbow animation, then fade in to a preset.
  enum class Phase { Normal, Animation, FadeIn };
  Phase phase = Phase::Normal;
  unsigned long phaseStartMs = 0;
  unsigned long lastAnimationFrameMs = 0;
  bool scheduledPending = false; // alarm fired while switch was off
  uint8_t fadeTargetWhite = 0;
  uint8_t fadeTargetRed = 0;
  uint8_t fadeTargetGreen = 0;
  uint8_t fadeTargetBlue = 0;

  bool pendingHwApply = false; // set by web-task setters, applied in update()

  // Guards whitePercent/redPercent/greenPercent/bluePercent/pendingHwApply,
  // which are written from the AsyncWebServer callback task (the setters
  // above) and read from the main loop() task (update()). Without this,
  // the four percent fields could be read mid-write - torn across two
  // tasks - producing a briefly wrong/stale color.
  portMUX_TYPE percentMux = portMUX_INITIALIZER_UNLOCKED;
};
