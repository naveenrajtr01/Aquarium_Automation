# Aquarium Lighting Controller (ESP32)

Controls a white LED strip (PWM dimmed) and an addressable RGB strip
(NeoPixel/WS2812B-style) for an aquarium, using:

- A physical on/off + preset-cycling switch
- A **"Koi Tank Controls" web dashboard**, hosted by the ESP32 itself over
  WiFi, reachable from any phone browser on your home network - no app
  install required (see [Web Dashboard](#web-dashboard-koi-tank-controls) below)
- A DS3231 real-time clock driving a daily evening lighting schedule (can be
  enabled/disabled from the dashboard)
- A buzzer that gives a brief chime on power-on and beeps on every mode change

Sketch folder: [AquariumLighting/](AquariumLighting)

## Hardware

| Signal                  | ESP32 Pin    | Notes                                                              |
|-------------------------|--------------|---------------------------------------------------------------------|
| White strip PWM out     | GPIO25       | Feeds your custom PWM dimming circuit, not the LED strip directly. Driven via `ledcSetup`/`ledcWrite` |
| RGB strip Data In       | GPIO27       | Direct digital data line to the addressable strip (30 pixels)       |
| On/Off + preset switch  | GPIO19       | Wire between the pin and GND. Internal pull-up is used in firmware  |
| Buzzer                  | GPIO32       | Active-high buzzer module, beeps 100 ms on power-on and any mode change |
| DS3231 SDA / SCL        | GPIO21 / 22  | I2C connection to the real-time clock module                       |

Change any of these in [AquariumLighting/Config.h](AquariumLighting/Config.h) if your wiring differs.
Also set `RGB_LED_COUNT` in `Config.h` to match the number of pixels on your strip.

## Behavior

### Switch (on/off + preset cycling)

- Switch **open** (not connected to GND) -> both strips off, always.
- Switch **closed** (connected to GND) -> both strips on, always.
- Every time the switch is closed (light turning back on), the controller
  advances to the next of the 3 presets (wrapping around) and stores the new
  index in flash (NVS) - a deliberate "next preset" step on every on-cycle.
- On power-up/reboot, the controller restores the last-stored preset without
  advancing it, regardless of which position the switch happens to be in at
  boot.
- Every preset change (switch-triggered or the automatic schedule below)
  triggers a 100 ms buzzer beep. The board also gives a single 100 ms beep
  on power-on/reset.

### Daily schedule (DS3231 alarm)

- The DS3231's built-in alarm (polled each loop, no extra interrupt wiring
  needed) fires once a day at `SCHEDULE_HOUR:SCHEDULE_MINUTE:SCHEDULE_SECOND`
  in `Config.h` (default **18:00 / 6:00 PM**), only while the schedule is
  enabled (toggle in the dashboard; enabled by default).
- If the switch is **on** when it fires: a 5-second RGB rainbow animation
  plays, then the light settles into `SCHEDULE_DEFAULT_PRESET_INDEX`'s preset
  (default index `0`), gradually glowing up from off over 2 seconds.
- If the switch is **off** when it fires, the sequence is queued and runs
  as soon as the switch is turned back on.
- **Time accuracy**: uploading a sketch takes long enough that setting the
  RTC from the compile-time `__DATE__`/`__TIME__` (as many examples do)
  ends up several seconds behind real time by the time it actually runs. To
  avoid that, the compile-time set is only ever used as a first-boot
  fallback (RTC never set / lost battery backup) - use the dashboard's
  **"Sync time from phone"** button for the accurate way to set it.

### Presets

Defined in [AquariumLighting/Presets.cpp](AquariumLighting/Presets.cpp) as a white brightness
percentage plus a red/green/blue percentage for the RGB strip. Edit that file
to change the 3 default presets (`Daylight`, `Sunset`, `Moonlight`).

### Dashboard control

- Brightness/color/preset values sent from the dashboard **only apply while
  the light is on**; controls are shown disabled (with a tap-to-explain
  toast) while the switch has the light off.
- Turning the light off and back on (whether via the switch or by leaving it
  off a long time) **discards any dashboard customization** and reapplies the
  currently stored preset - customizing color/brightness from the dashboard
  is a temporary override on top of a preset, not a new saved state.

## Web Dashboard (Koi Tank Controls)

1. Set your WiFi credentials in [AquariumLighting/Config.h](AquariumLighting/Config.h):
   `WIFI_SSID` / `WIFI_PASSWORD`.
2. After uploading, open the Serial monitor - it logs the IP address and,
   if mDNS started successfully, a friendly hostname:
   `http://koitank.local/` (or `http://<esp32-ip>/` if your phone/router
   doesn't support mDNS - most modern Android/iOS do).
3. Your phone must be on the **same WiFi network** as the ESP32 (this is a
   local-network-only dashboard, not exposed to the internet).
4. Log in with the Basic Auth credentials from `Config.h`
   (`WEB_AUTH_USERNAME` / `WEB_AUTH_PASSWORD`, default `admin` / `admin`).

Dashboard features:

- **Status**: read-only on/off badge, active preset name, and the current
  ESP32/DS3231 time (ticks live between polls). On/off is intentionally
  **not** a dashboard control - only the physical switch turns the light
  on/off. A **"Sync time from phone"** button sets the DS3231 from the
  browser's clock.
- **Preset**: buttons to jump directly to any of the 3 presets. This
  **only takes effect while the light is on** - if it's off, the preset,
  white brightness and RGB color cards are shown dimmed/disabled and tapping
  them shows a toast explaining why.
- **White brightness**: a slider, 0-100%.
- **RGB color**: a gradient square (drag to pick saturation/brightness) plus
  a hue slider, similar to the font-color pickers in document editors. The
  square's vertical axis doubles as brightness - consistent with this
  project's [existing design](#why-rgb-brightness-is-folded-into-rg-directly)
  of folding brightness directly into the R/G/B percentages rather than a
  separate brightness control.
- **Daily schedule**: a toggle to enable/disable the schedule, and an
  editable time picker (replaces the old compile-time-only
  `SCHEDULE_HOUR`/`SCHEDULE_MINUTE`, disabled while the schedule is off) -
  saving reprograms the DS3231 alarm immediately and persists both the time
  and enabled state to flash (NVS), so they survive reboots.
- **Live logs**: a toggle that opens a WebSocket only while switched on, so
  log streaming to your phone is fully opt-in and stops the instant you
  switch it off (same log lines as Serial, see [Logging](#logging)).

Required libraries (see [Building & Uploading](#building--uploading-arduino-ide)):
- **ESPAsyncWebServer** and its dependency **AsyncTCP** (install both via
  Arduino Library Manager). `WiFi.h`/`ESPmDNS.h` are bundled with the ESP32
  core.

Known limitation: browsers don't reliably attach cached Basic Auth
credentials to a WebSocket handshake, so the `/ws/logs` stream's server-side
auth check may not always succeed even when logged into the page - this is a
browser API limitation, not a bug in the sketch.

## Why RGB brightness is folded into R/G/B directly

The Adafruit_NeoPixel library's `setBrightness()` rescales stored pixel data
in place and loses color precision on repeated changes, and it isn't a
separate independent control from the RGB values. Instead, this project
treats each channel's 0-100% value as directly defining that channel's 8-bit
intensity (`0-255`) - color and brightness are the same control, exactly as
requested. See [AquariumLighting/RgbStrip.cpp](AquariumLighting/RgbStrip.cpp).

## File Structure

```
AquariumLighting/
  AquariumLighting.ino   Sketch entry point: setup()/loop(), wires all modules together
  Config.h               All pin numbers and timing constants in one place
  WhiteStrip.h/.cpp       PWM control of the white LED strip via ESP32 LEDC
  RgbStrip.h/.cpp         Addressable RGB strip control via Adafruit_NeoPixel
  SwitchInput.h/.cpp      Debounced on/off switch reader, reports TurnedOn/TurnedOff events
  Buzzer.h/.cpp           Non-blocking 100 ms beep on power-on and mode changes
  RtcManager.h/.cpp       DS3231 wrapper: time keeping, dashboard-driven time set,
                          daily alarm with an enable/disable flag
  Presets.h/.cpp          The 3 preset definitions (white % + RGB %)
  StateManager.h/.cpp     Core logic: preset selection/cycling, persistence (NVS),
                          on/off state, the scheduled animation/fade-in sequence,
                          applies values to WhiteStrip/RgbStrip/Buzzer, validates
                          dashboard writes
  WebDashboard.h/.cpp     Hosts the "Koi Tank Controls" WiFi web dashboard (WiFi connect,
                          HTTP+WebSocket server, Basic Auth, embedded HTML/CSS/JS UI)
  Logger.h/.cpp           Mirrors log lines to Serial and (while a client has it open)
                          the dashboard's live log stream
```

Responsibilities are kept single-purpose per file so each hardware concern
(white PWM, RGB strip, switch, buzzer, RTC) can be read/edited
independently of the others. `StateManager` is the only place that decides
*what* the lights show; the other modules only know how to drive their piece
of hardware or report raw input.

## Logging

All significant events - boot steps, switch/preset changes, the schedule
alarm firing, and RTC time changes - are logged via
[AquariumLighting/Logger.h](AquariumLighting/Logger.h), which writes to both:
- The **Serial monitor** at 115200 baud.
- The dashboard's **live log stream** (see [Web Dashboard](#web-dashboard-koi-tank-controls)
  above), so you can see the same log lines in a phone browser without a USB cable.

## Building & Uploading (Arduino IDE)

1. Install the **ESP32 boards package** (Tools > Board > Boards Manager, search
   "esp32", install the Espressif package) if not already installed.
2. Install the **Adafruit NeoPixel** library (Tools > Manage Libraries, search
   "Adafruit NeoPixel", install it and its dependency "Adafruit BusIO" if
   prompted).
3. Install the **RTClib** library (by Adafruit; Tools > Manage Libraries,
   search "RTClib", install it and its dependency "Adafruit BusIO" if
   prompted) - used to talk to the DS3231.
4. Install **ESPAsyncWebServer** and its dependency **AsyncTCP** (Tools >
   Manage Libraries, search each by name) - used by the web dashboard.
5. `Preferences.h`, `Wire.h`, `WiFi.h` and `ESPmDNS.h` are bundled with the
   ESP32 core - no separate install needed.
6. Open `AquariumLighting/AquariumLighting.ino` in the Arduino IDE.
7. Set `WIFI_SSID`/`WIFI_PASSWORD` in `Config.h` for the web dashboard (see
   [Web Dashboard](#web-dashboard-koi-tank-controls) above).
8. Select your ESP32 board and port under Tools, then set **Tools > Partition
   Scheme** to **"Huge APP (3MB No OTA/1MB SPIFFS)"** (or "No OTA (2MB
   APP/2MB SPIFFS)"). This project doesn't use SPIFFS/LittleFS, so that
   space isn't needed anyway.
9. Adjust `RGB_LED_COUNT` and any pin numbers in `Config.h` to match your
   hardware before uploading.
10. After the first upload, use the dashboard's **"Sync time from phone"**
    button so the DS3231 has accurate time - the compile-time fallback is
    only approximate.

## Customizing

- **Presets**: edit the `PRESETS` array in `Presets.cpp`.
- **Pins / timing**: edit `Config.h`.
- **Switch debounce window**: `SWITCH_DEBOUNCE_MS` in `Config.h` (default 400 ms).
  If a light/incidental touch on the switch still causes unwanted toggles
  after raising this, it's noise pickup on the wire rather than bounce - add
  a ~100 nF ceramic capacitor between the switch pin and GND.
- **Schedule time**: `SCHEDULE_HOUR` / `SCHEDULE_MINUTE` / `SCHEDULE_SECOND`
  in `Config.h` are the first-boot defaults (18:00:00) - once set via the web
  dashboard's schedule editor, the chosen hour/minute is persisted to flash
  (NVS) and takes over from then on.
- **Web dashboard credentials/branding**: `WEB_AUTH_USERNAME`,
  `WEB_AUTH_PASSWORD`, `DASHBOARD_MDNS_HOSTNAME` in `Config.h`.
- **Animation/fade-in durations**: `SCHEDULE_ANIMATION_MS` (default 10 s) and
  `SCHEDULE_FADEIN_MS` (default 2 s) in `Config.h`.
- **Preset applied after the schedule animation**:
  `SCHEDULE_DEFAULT_PRESET_INDEX` in `Config.h` (default `0`).
- **Buzzer beep length**: `BUZZER_BEEP_MS` in `Config.h` (default 100 ms).
