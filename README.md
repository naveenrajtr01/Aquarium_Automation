# Aquarium Lighting Controller (ESP32)

Controls a white LED strip (PWM dimmed) and an addressable RGB strip
(NeoPixel/WS2812B-style) for an aquarium, using:

- A physical on/off + preset-cycling switch
- A mobile app connected over Bluetooth Low Energy (BLE)
- A DS3231 real-time clock driving a daily evening lighting schedule
- A buzzer that gives a brief chime on power-on and beeps on every mode change

Sketch folder: [AquariumLighting/](AquariumLighting)

## Hardware

| Signal                  | ESP32 Pin    | Notes                                                              |
|-------------------------|--------------|---------------------------------------------------------------------|
| White strip PWM out     | GPIO25       | Feeds your custom PWM dimming circuit, not the LED strip directly. Driven via `ledcSetup`/`ledcWrite` |
| RGB strip Data In       | GPIO27       | Direct digital data line to the addressable strip (30 pixels)       |
| On/Off + preset switch  | GPIO19       | Wire between the pin and GND. Internal pull-up is used in firmware  |
| Buzzer                  | GPIO32       | Active-high buzzer module, beeps 300 ms on power-on and any mode change |
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
  triggers a 300 ms buzzer beep. The board also gives a single 300 ms beep
  on power-on/reset.

### Daily schedule (DS3231 alarm)

- The DS3231's built-in alarm (polled each loop, no extra interrupt wiring
  needed) fires once a day at `SCHEDULE_HOUR:SCHEDULE_MINUTE:SCHEDULE_SECOND`
  in `Config.h` (default **18:00 / 6:00 PM**).
- If the switch is **on** when it fires: a 10-second RGB rainbow animation
  plays, then the light settles into `SCHEDULE_DEFAULT_PRESET_INDEX`'s preset
  (default index `0`), gradually glowing up from off over 2 seconds.
- If the switch is **off** when it fires, the sequence is queued and runs
  as soon as the switch is turned back on.
- **Time accuracy**: uploading a sketch takes long enough that setting the
  RTC from the compile-time `__DATE__`/`__TIME__` (as many examples do)
  ends up several seconds behind real time by the time it actually runs. To
  avoid that, the compile-time set is only ever used as a first-boot
  fallback (RTC never set / lost battery backup) - see the "Set Time" BLE
  characteristic below for the accurate way to set it.

### Presets

Defined in [AquariumLighting/Presets.cpp](AquariumLighting/Presets.cpp) as a white brightness
percentage plus a red/green/blue percentage for the RGB strip. Edit that file
to change the 3 default presets (`Daylight`, `Sunset`, `Moonlight`).

### BLE app control

- Brightness/color values sent from the app **only apply while the light is
  on**; writes are ignored while the switch has the light off.
- Turning the light off and back on (whether via the switch or by leaving it
  off a long time) **discards any BLE customization** and reapplies the
  currently stored preset - customizing color/brightness from the app is a
  temporary override on top of a preset, not a new saved state.
- The app can read: on/off state, active preset index, and the current
  white/red/green/blue percentages (0-100 each).

See BLE protocol details below. For step-by-step instructions on reading/
writing values and viewing logs with Nordic's nRF Connect app (iOS), see
[NRF_CONNECT_GUIDE.md](NRF_CONNECT_GUIDE.md).

## BLE Protocol

Device advertises as **`Aquarium Light`** with one custom GATT service.

Service UUID: `24360cf0-21b2-4948-ae20-246a7df5695b`

All values are a single unsigned byte (`uint8`).

| Characteristic | UUID                                   | Properties          | Range / meaning                          |
|-----------------|-----------------------------------------|----------------------|-------------------------------------------|
| On/Off state    | `321675d4-daa7-49d7-8fdf-79d83f62b51e` | Read, Notify         | `0` = off, `1` = on                       |
| Active preset   | `70796f44-9ba4-481b-a846-bc1e16a00342` | Read, Notify         | `0`-`2` (index into the 3 presets)        |
| White brightness| `f1af25c5-8e16-41e3-b5f9-b7caeaef25d3` | Read, Write, Notify  | `0`-`100` (%)                              |
| Red             | `6c6468b6-7a0d-4c16-88be-8f8bce4a0e68` | Read, Write, Notify  | `0`-`100` (%)                              |
| Green           | `df416687-5b23-4be2-a479-ca5abbea83f4` | Read, Write, Notify  | `0`-`100` (%)                              |
| Blue            | `73d9d82e-de50-424b-b91b-bd90e502758a` | Read, Write, Notify  | `0`-`100` (%)                              |
| Set Time        | `8f2ac0d1-3e1a-4c8b-9a2f-6b1d5e8c4a2b` | Write                | `uint32`, little-endian Unix epoch seconds |
| Debug Log       | `b3d9a6e2-7c44-4b6a-9a3d-1f6e2c9a7d10` | Read, Notify         | UTF-8 text, one log line per notification  |

Notes:
- All read/notify characteristics have a CCCD (`0x2902`) so an app can
  subscribe to notifications and stay in sync without polling.
- Every value change - switch on/off, preset changes, BLE writes, and the
  scheduled animation/fade-in (throttled to every ~150 ms while it's actively
  ramping) - is pushed to all read/notify characteristics, so a subscribed
  app always converges to the true current state.
- Writing to White/Red/Green/Blue while the light is off is rejected: the
  characteristic is reset back to its actual current value and no change is
  applied - so the app should treat a notify-without-matching-write as "my
  change was rejected."
- Red/Green/Blue are independent channels; send all three (in combination)
  to mix a color, same as normal RGB color mixing.
- **Set Time**: write the current Unix epoch time (seconds since
  1970-01-01 UTC) as a little-endian `uint32` to set the DS3231 accurately.
  Since the phone's clock is normally NTP-synced, this avoids the time skew
  caused by upload latency that a compile-time set would have. Send this
  once after pairing (and any time you suspect drift).
- **Debug Log**: subscribe to this characteristic to receive the same log
  lines that are printed to the Serial monitor (setup steps, switch/preset
  changes, schedule events, RTC time, every accepted/rejected BLE write,
  etc.) - useful for checking behavior without a USB connection.

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
  Config.h               All pin numbers, timing constants, and BLE UUIDs in one place
  WhiteStrip.h/.cpp       PWM control of the white LED strip via ESP32 LEDC
  RgbStrip.h/.cpp         Addressable RGB strip control via Adafruit_NeoPixel
  SwitchInput.h/.cpp      Debounced on/off switch reader, reports TurnedOn/TurnedOff events
  Buzzer.h/.cpp           Non-blocking 300 ms beep on power-on and mode changes
  RtcManager.h/.cpp       DS3231 wrapper: time keeping, BLE-driven time set, daily alarm
  Presets.h/.cpp          The 3 preset definitions (white % + RGB %)
  StateManager.h/.cpp     Core logic: preset selection/cycling, persistence (NVS),
                          on/off state, the scheduled animation/fade-in sequence,
                          applies values to WhiteStrip/RgbStrip/Buzzer, validates BLE writes
  BleController.h/.cpp    BLE GATT server: exposes StateManager's state to the app,
                          forwards write/set-time requests, and streams the debug log
  Logger.h/.cpp           Mirrors log lines to Serial and the Debug Log BLE characteristic
```

Responsibilities are kept single-purpose per file so each hardware concern
(white PWM, RGB strip, switch, buzzer, RTC, BLE) can be read/edited
independently of the others. `StateManager` is the only place that decides
*what* the lights show; the other modules only know how to drive their piece
of hardware or report raw input.

## Logging

All significant events - boot steps, switch/preset changes, the schedule
alarm firing, RTC time changes, rejected BLE writes, and BLE
connect/disconnect - are logged via [AquariumLighting/Logger.h](AquariumLighting/Logger.h),
which writes to both:
- The **Serial monitor** at 115200 baud.
- The **Debug Log** BLE characteristic (see BLE Protocol below), so you can
  see the same log lines in a phone app without a USB cable.

## Building & Uploading (Arduino IDE)

1. Install the **ESP32 boards package** (Tools > Board > Boards Manager, search
   "esp32", install the Espressif package) if not already installed.
2. Install the **Adafruit NeoPixel** library (Tools > Manage Libraries, search
   "Adafruit NeoPixel", install it and its dependency "Adafruit BusIO" if
   prompted).
3. Install the **RTClib** library (by Adafruit; Tools > Manage Libraries,
   search "RTClib", install it and its dependency "Adafruit BusIO" if
   prompted) - used to talk to the DS3231.
4. `Preferences.h`, `Wire.h` and the `BLEDevice`/`BLEServer`/etc. headers are
   bundled with the ESP32 core - no separate install needed.
5. Open `AquariumLighting/AquariumLighting.ino` in the Arduino IDE.
6. Select your ESP32 board and port under Tools, then Upload.
7. Adjust `RGB_LED_COUNT` and any pin numbers in `Config.h` to match your
   hardware before uploading.
8. After the first upload, write the current time to the **Set Time** BLE
   characteristic from your app so the DS3231 has accurate time (see BLE
   Protocol above) - the compile-time fallback is only approximate.

## Customizing

- **Presets**: edit the `PRESETS` array in `Presets.cpp`.
- **Pins / timing / BLE UUIDs**: edit `Config.h`.
- **Switch debounce window**: `SWITCH_DEBOUNCE_MS` in `Config.h` (default 150 ms).
- **Schedule time**: `SCHEDULE_HOUR` / `SCHEDULE_MINUTE` / `SCHEDULE_SECOND`
  in `Config.h` (default 18:00:00).
- **Animation/fade-in durations**: `SCHEDULE_ANIMATION_MS` (default 10 s) and
  `SCHEDULE_FADEIN_MS` (default 2 s) in `Config.h`.
- **Preset applied after the schedule animation**:
  `SCHEDULE_DEFAULT_PRESET_INDEX` in `Config.h` (default `0`).
- **Buzzer beep length**: `BUZZER_BEEP_MS` in `Config.h` (default 300 ms).
