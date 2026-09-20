# Aquarium Lighting Controller (ESP32)

Controls a white LED strip (PWM dimmed) and an addressable RGB strip
(NeoPixel/WS2812B-style) for an aquarium, using:

- A physical on/off + preset-cycling switch
- A mobile app connected over Bluetooth Low Energy (BLE)

Sketch folder: [AquariumLighting/](AquariumLighting)

## Hardware

| Signal                  | ESP32 Pin | Notes                                                              |
|-------------------------|-----------|---------------------------------------------------------------------|
| White strip PWM out     | GPIO25    | Feeds your custom PWM dimming circuit, not the LED strip directly   |
| RGB strip Data In       | GPIO26    | Direct digital data line to the addressable strip                  |
| On/Off + preset switch  | GPIO27    | Wire between the pin and GND. Internal pull-up is used in firmware  |

Change any of these in [AquariumLighting/Config.h](AquariumLighting/Config.h) if your wiring differs.
Also set `RGB_LED_COUNT` in `Config.h` to match the number of pixels on your strip.

## Behavior

### Switch (on/off + preset cycling)

- Switch **open** -> both strips off.
- Switch **closed** -> both strips on, showing the active preset.
- If you open and close the switch again **within 5 minutes**
  (`QUICK_TOGGLE_THRESHOLD_MS` in `Config.h`), it's treated as a deliberate
  "next preset" gesture: the controller advances to the next of the 3
  presets (wrapping around) and stores the new index in flash (NVS).
- If the switch is closed after being open **longer** than that (e.g. lights
  left off overnight), the controller does **not** advance the preset - it
  simply restores the last-used preset.
- On power-up/reboot, the controller always behaves like the "long time off"
  case: it restores the stored preset and never advances it, regardless of
  which position the switch happens to be in at boot.

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

See BLE protocol details below.

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

Notes:
- All characteristics have a CCCD (`0x2902`) so an app can subscribe to
  notifications and stay in sync without polling.
- Writing to White/Red/Green/Blue while the light is off is rejected: the
  characteristic is reset back to its actual current value and no change is
  applied - so the app should treat a notify-without-matching-write as "my
  change was rejected."
- Red/Green/Blue are independent channels; send all three (in combination)
  to mix a color, same as normal RGB color mixing.

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
  Presets.h/.cpp          The 3 preset definitions (white % + RGB %)
  StateManager.h/.cpp     Core logic: preset selection/cycling, persistence (NVS),
                          on/off state, applies values to WhiteStrip/RgbStrip,
                          validates BLE writes
  BleController.h/.cpp    BLE GATT server: exposes StateManager's state to the app
                          and forwards write requests back into it
```

Responsibilities are kept single-purpose per file so each hardware concern
(white PWM, RGB strip, switch, BLE) can be read/edited independently of the
others. `StateManager` is the only place that decides *what* the lights show;
the other modules only know how to drive their piece of hardware or report
raw input.

## Building & Uploading (Arduino IDE)

1. Install the **ESP32 boards package** (Tools > Board > Boards Manager, search
   "esp32", install the Espressif package) if not already installed.
2. Install the **Adafruit NeoPixel** library (Tools > Manage Libraries, search
   "Adafruit NeoPixel", install it and its dependency "Adafruit BusIO" if
   prompted).
3. `Preferences.h` and the `BLEDevice`/`BLEServer`/etc. headers are bundled
   with the ESP32 core - no separate install needed.
4. Open `AquariumLighting/AquariumLighting.ino` in the Arduino IDE.
5. Select your ESP32 board and port under Tools, then Upload.
6. Adjust `RGB_LED_COUNT` and any pin numbers in `Config.h` to match your
   hardware before uploading.

## Customizing

- **Presets**: edit the `PRESETS` array in `Presets.cpp`.
- **Pins / timing / BLE UUIDs**: edit `Config.h`.
- **Quick-toggle window** (how long a "deliberate" preset-cycle press can be):
  `QUICK_TOGGLE_THRESHOLD_MS` in `Config.h`.
