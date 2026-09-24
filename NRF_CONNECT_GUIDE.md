# Controlling the Aquarium Light with nRF Connect for Mobile (iOS)

This guide walks through using Nordic's **nRF Connect for Mobile** app on
iOS to read/write the aquarium light's settings and view its live log,
without needing a USB cable or the custom app. See [README.md](README.md)
for the full BLE protocol reference this guide is based on.

## 1. Install and connect

1. Install **nRF Connect for Mobile** from the iOS App Store (by Nordic
   Semiconductor).
2. Power on the board. It advertises as **`Aquarium Light`**.
3. Open the app -> **Scanner** tab -> find `Aquarium Light` in the list ->
   tap **Connect**.
4. iOS negotiates the BLE connection parameters and ATT MTU automatically
   (the firmware requests an MTU of 185 bytes, enough for full log lines) -
   no manual MTU step is needed on iOS, unlike Android.
5. After connecting you'll land on the **Client** view showing one service:

   - Service UUID: `24360cf0-21b2-4948-ae20-246a7df5695b`

   Tap it to expand and see all 8 characteristics below.

## 2. Characteristic cheat sheet

All numeric characteristics are a single raw byte (`uint8`) unless noted.
In nRF Connect, each characteristic row has icons for its supported
operations - an upload arrow (Write), a download arrow (Read), and
multiple-down-arrows (Enable notifications).

| Characteristic  | UUID (last group)  | Ops                | Value                                      |
|------------------|---------------------|---------------------|---------------------------------------------|
| On/Off state     | `...f62b51e`        | Read, Notify        | `00` = off, `01` = on                        |
| Active preset    | `...16a00342`       | Read, Notify        | `00`-`02` (index into the 3 presets)         |
| White brightness | `...caeaef25d3`     | Read, Write, Notify | `00`-`64` hex (0-100 %)                      |
| Red              | `...bce4a0e68`      | Read, Write, Notify | `00`-`64` hex (0-100 %)                      |
| Green            | `...5abbea83f4`     | Read, Write, Notify | `00`-`64` hex (0-100 %)                      |
| Blue             | `...d90e502758a`    | Read, Write, Notify | `00`-`64` hex (0-100 %)                      |
| Set Time         | `...6b1d5e8c4a2b`   | Write               | `uint32`, little-endian Unix epoch seconds   |
| Debug Log        | `...1f6e2c9a7d10`   | Read, Notify        | UTF-8 text (one log line per notification)   |

Full UUIDs are in [README.md](README.md#ble-protocol).

## 3. Reading a value

1. Tap the characteristic's **download arrow** (Read icon).
2. The value appears under the characteristic. By default nRF Connect shows
   raw bytes - tap the small text/format toggle next to the value (or the
   three-dot menu -> **"Show as"**) and switch to a numeric format:
   - Percent/on-off/preset characteristics: show as **UINT8**.
   - Debug Log: show as **Text (UTF-8)**.

## 4. Writing a value (e.g. set white brightness to 75%)

1. Tap the characteristic's **upload arrow** (Write icon).
2. In the value editor, switch the input type from "Byte array" to
   **UINT8**, then enter a decimal value `0`-`100` (e.g. `75`). If your
   version of the app only accepts hex, convert first (75 -> `4B`).
3. Tap **Send**. If the light is currently **off**, the write is ignored by
   the firmware and the characteristic's notified value will not change -
   turn the light on (physical switch closed) before writing brightness/
   color values.
4. Red/Green/Blue are independent - write all three to mix a color, the
   same as normal RGB color mixing.

### Setting the time (Set Time characteristic)

The firmware needs the current Unix epoch time (seconds since
1970-01-01 UTC) as a **little-endian uint32** (4 bytes):

1. Get the current epoch time, e.g. from a phone terminal/shortcut or
   https://www.unixtimestamp.com (avoid searching for this on a device that
   shouldn't have internet access - any phone clock/calculator works too:
   epoch = days-since-1970 math, or just use an offline "Unix time" app).
2. Convert it to 4 little-endian bytes. Example: epoch `1735000000` in hex
   is `67 76 F3 80` (big-endian); reversed for little-endian that's
   `80 F3 76 67`.
3. In nRF Connect, switch the Set Time characteristic's write editor to
   **"Byte array"** and enter the 4 little-endian bytes (e.g.
   `80-F3-76-67`), then **Send**.
4. Do this once after pairing, and again any time you suspect the DS3231
   has drifted.

## 5. Viewing live logs (Debug Log characteristic)

1. Find the **Debug Log** characteristic (UUID ending `...1f6e2c9a7d10`).
2. Tap the **multiple-down-arrows icon** to enable notifications
   (subscribes to its CCCD `0x2902`) - this mirrors what would otherwise
   only be visible on the Serial monitor.
3. Every log line the firmware emits (boot steps, switch/preset changes,
   schedule events, RTC time, rejected BLE writes, etc.) arrives as a
   separate notification.
4. Set the display format for this characteristic to **Text (UTF-8)** (via
   the format toggle/three-dot menu) so lines are readable instead of raw
   hex bytes.
5. Optional: nRF Connect can log all notifications to a file (three-dot
   menu -> **Log** / export) if you want a persistent record instead of
   watching the live feed.

## 6. Notes / troubleshooting

- If a write to White/Red/Green/Blue seems to silently "not stick," check
  the On/Off characteristic first - the light must be on for writes to
  apply.
- Only one central (phone) can hold the connection at a time; if the
  custom mobile app is also trying to connect, disconnect one side first.
- If notifications stop arriving after the app is backgrounded for a long
  time, pull down to reconnect or toggle Bluetooth - this is an iOS
  background BLE limitation, not a firmware issue.
