#include "Presets.h"

// Edit these three entries to customize your presets.
// { name, whitePercent, redPercent, greenPercent, bluePercent }
const Preset PRESETS[NUM_PRESETS] = {
  { "Daylight",  100,   0,   0,   0 },  // Bright white, RGB strip off
  { "Sunset",     30, 100,  20,  60 },  // Dim white + warm purple/orange RGB blend
  { "Moonlight",   0,   0,   0,  15 },  // White off, faint blue RGB for night viewing
};
