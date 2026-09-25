#pragma once
// ---------------------------------------------------------------------------
// Central place for every pin assignment and timing constant.
// Change values here rather than hunting through the other modules.
// ---------------------------------------------------------------------------

// ---- White LED strip (PWM via custom dimming circuit) ----
#define WHITE_LED_PWM_PIN          25      // GPIO25 / D25
#define WHITE_PWM_FREQ_HZ          5000    // PWM frequency, flicker-free for most dimmer circuits
#define WHITE_PWM_RESOLUTION_BITS  8       // 0-255 duty range

// ---- Addressable RGB strip (NeoPixel-compatible, e.g. WS2812B) ----
#define RGB_DATA_PIN               27      // GPIO27 drives the strip's Data-In
#define RGB_LED_COUNT              30      // <-- CHANGE THIS to match your strip's pixel count
// Byte order on the data wire - must match your specific strip's chip, not
// just "WS2812B" as a family. Solved from field testing: Blue-only lit
// Green under NEO_GRB, then Red under NEO_GBR - working the permutation
// back from those two results gives the strip's true order as BRG.
#define RGB_LED_TYPE               (NEO_BRG + NEO_KHZ800)
// White-balance correction: pure red/green/blue tested correct, but any
// mixed color reads green/blue-shifted (e.g. gold/amber looked greenish,
// warm white looked blueish) - the classic WS2812 symptom of green (then
// blue) LEDs being visually brighter than red at equal 0-255 values.
// FastLED's well-known "TypicalLEDStrip" factors (0xFFB0F0) were tried as
// static values first and weren't a strong enough correction for this
// strip - these are now just the initial/fallback defaults; the real
// values are live-tunable from the dashboard's calibration sliders and
// persisted in NVS (see PREFS_KEY_WB_* below), so they no longer need a
// firmware reflash to adjust.
#define RGB_CHANNEL_R_SCALE        255
#define RGB_CHANNEL_G_SCALE        176
#define RGB_CHANNEL_B_SCALE        240
// WS2812's single-wire protocol occasionally drops/corrupts a bit in transit
// (RF/EMI on the data line, marginal 3.3V->5V logic level, etc.), leaving a
// random stretch of pixels stuck showing stale color until the next full
// re-send - confirmed by field testing where toggling the switch or
// re-picking the same preset always clears it. Rather than rely on the user
// noticing and re-triggering, StateManager re-sends the strip's current
// (on or off) color on this interval so any glitch self-heals within a few
// seconds.
#define RGB_SELF_HEAL_INTERVAL_MS  3000UL

// ---- On/Off + preset-cycling switch ----
#define SWITCH_PIN                 19      // Switch wired between this pin and GND (internal pull-up used)
// Mechanical debounce window: the raw reading must stay stable for this long
// before a transition is accepted. Raised well above typical <20ms contact
// bounce so a light/incidental touch (which can leave the pin noisy or
// briefly mis-read for a while, especially with the ESP32's weak internal
// pull-up) doesn't get mistaken for a deliberate flip. If unwanted toggles
// still happen, it points to noise pickup on the switch wire rather than
// bounce - fix in hardware with a ~100 nF ceramic capacitor between
// SWITCH_PIN and GND, right at the pin.
#define SWITCH_DEBOUNCE_MS         400UL

// ---- Buzzer (mode-change feedback) ----
#define BUZZER_PIN                 32      // Active-high buzzer module
#define BUZZER_BEEP_MS             100UL   // Beep length on power-on and any mode change

// ---- DS3231 real-time clock (I2C) ----
#define RTC_SDA_PIN                21
#define RTC_SCL_PIN                22

// ---- Daily lighting schedule (DS3231 alarm) ----
// At this time every day, the light (if the switch is ON) plays a short RGB
// animation, then settles into SCHEDULE_DEFAULT_PRESET_INDEX, gradually
// glowing up from off. If the switch is OFF when the alarm fires, the
// sequence is queued and runs as soon as the switch is turned back on.
#define SCHEDULE_HOUR              18      // 24-hour format - 18 = 6:00 PM
#define SCHEDULE_MINUTE            0
#define SCHEDULE_SECOND            0
#define SCHEDULE_ANIMATION_MS      5000UL  // rainbow animation duration
#define SCHEDULE_ANIMATION_FRAME_MS   30UL // redraw interval during the animation
#define SCHEDULE_ANIMATION_ROTATE_MS   8UL // ms per hue-wheel step (lower = faster spin)
#define SCHEDULE_FADEIN_MS         2000UL  // gradual glow-up duration after the animation
#define SCHEDULE_DEFAULT_PRESET_INDEX 0    // preset applied after the scheduled animation

// ---- Presets ----
#define NUM_PRESETS                3

// ---- Persistent storage (NVS via Preferences) ----
#define PREFS_NAMESPACE            "aqualight"
#define PREFS_KEY_PRESET_INDEX     "presetIdx"
#define PREFS_KEY_SCHEDULE_HOUR    "schedHour"
#define PREFS_KEY_SCHEDULE_MINUTE  "schedMin"
#define PREFS_KEY_SCHEDULE_ENABLED "schedEn"
#define PREFS_KEY_WB_RED           "wbRed"
#define PREFS_KEY_WB_GREEN         "wbGreen"
#define PREFS_KEY_WB_BLUE          "wbBlue"

// ---- WiFi (for the on-device web dashboard) ----
#define WIFI_SSID                  "YOUR_WIFI_SSID"      // <-- CHANGE THIS
#define WIFI_PASSWORD              "YOUR_WIFI_PASSWORD"  // <-- CHANGE THIS

// ---- Web dashboard (local network only) ----
#define WEB_SERVER_PORT            80

#define WEB_AUTH_ENABLED           false   // set true to require the login below
#define WEB_AUTH_USERNAME          "admin"
#define WEB_AUTH_PASSWORD          "admin"
#define DASHBOARD_MDNS_HOSTNAME    "koitank"              // http://koitank.local/

