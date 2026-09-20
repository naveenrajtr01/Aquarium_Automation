#pragma once
#include <Arduino.h>
#include "Config.h"

// A preset bundles a white-strip brightness with an RGB-strip color, each
// channel expressed directly as a 0-100% value (no separate 0-255 color +
// brightness split - percent values fully determine both hue and intensity).
struct Preset {
  const char *name;
  uint8_t whitePercent;
  uint8_t redPercent;
  uint8_t greenPercent;
  uint8_t bluePercent;
};

// Default set of 3 presets. Edit freely in Presets.cpp - order defines the
// cycle order used when the switch is quick-toggled.
extern const Preset PRESETS[NUM_PRESETS];
