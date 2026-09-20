#pragma once
// ---------------------------------------------------------------------------
// Central place for every pin assignment, timing constant and BLE UUID.
// Change values here rather than hunting through the other modules.
// ---------------------------------------------------------------------------

// ---- White LED strip (PWM via custom dimming circuit) ----
#define WHITE_LED_PWM_PIN          25      // GPIO25 / D25
#define WHITE_PWM_CHANNEL          0       // ESP32 LEDC channel
#define WHITE_PWM_FREQ_HZ          5000    // PWM frequency, flicker-free for most dimmer circuits
#define WHITE_PWM_RESOLUTION_BITS  8       // 0-255 duty range

// ---- Addressable RGB strip (NeoPixel-compatible, e.g. WS2812B) ----
#define RGB_DATA_PIN               26      // GPIO26 drives the strip's Data-In
#define RGB_LED_COUNT              30      // <-- CHANGE THIS to match your strip's pixel count
#define RGB_LED_TYPE               (NEO_GRB + NEO_KHZ800)

// ---- On/Off + preset-cycling switch ----
#define SWITCH_PIN                 27      // Switch wired between this pin and GND (internal pull-up used)
#define SWITCH_DEBOUNCE_MS         50UL    // Mechanical debounce window
// If the switch is closed (light back ON) within this long after being opened,
// it counts as a "quick toggle" and advances to the next preset. Any longer gap
// (e.g. lights left off overnight) restores the last-used preset instead.
#define QUICK_TOGGLE_THRESHOLD_MS  (5UL * 60UL * 1000UL)  // 5 minutes

// ---- Presets ----
#define NUM_PRESETS                3

// ---- Persistent storage (NVS via Preferences) ----
#define PREFS_NAMESPACE            "aqualight"
#define PREFS_KEY_PRESET_INDEX     "presetIdx"

// ---- Bluetooth Low Energy ----
#define BLE_DEVICE_NAME            "Aquarium Light"

// Custom 128-bit UUIDs generated for this project (random, no external meaning).
#define SERVICE_UUID       "24360cf0-21b2-4948-ae20-246a7df5695b"
#define CHAR_ONOFF_UUID    "321675d4-daa7-49d7-8fdf-79d83f62b51e"  // read+notify, uint8 0/1
#define CHAR_PRESET_UUID   "70796f44-9ba4-481b-a846-bc1e16a00342"  // read+notify, uint8 0-2
#define CHAR_WHITE_UUID    "f1af25c5-8e16-41e3-b5f9-b7caeaef25d3"  // read+write+notify, uint8 0-100 (%)
#define CHAR_RED_UUID      "6c6468b6-7a0d-4c16-88be-8f8bce4a0e68"  // read+write+notify, uint8 0-100 (%)
#define CHAR_GREEN_UUID    "df416687-5b23-4be2-a479-ca5abbea83f4"  // read+write+notify, uint8 0-100 (%)
#define CHAR_BLUE_UUID     "73d9d82e-de50-424b-b91b-bd90e502758a"  // read+write+notify, uint8 0-100 (%)
