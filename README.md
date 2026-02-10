

Production-oriented ESP-IDF firmware scaffold for an ESP-based IR controller with:
Production-oriented scaffold for an ESP-based IR controller firmware with:

- Hierarchical model (`Home -> Room -> Device -> Remote -> Button`)
- Single trigger path (`trigger -> button_id -> IRButton -> send()`)
- JSON config + version/migration hooks
- Boot failure tracking + safe mode entry
- Module boundaries for IR, storage, UI, network, and system reliability

See `docs/architecture.md` for architecture details.

## Current Repository Status

This repository currently provides the **firmware architecture scaffold** (models, interfaces, config schema, boot/safe-mode logic) but does **not yet include board-specific driver wiring** or a full build target (`CMakeLists.txt`, `platformio.ini`, pin map, HAL glue).

That means installation/flashing steps below are split into:
1. **Host-side validation now** (works immediately).
2. **Device flashing flow** (to use once you add board integration layer).

## Installation

### 1) Clone

```bash
git clone https://github.com/prudvireddy3/esp-ir.git esp-ir
cd esp-ir
```

### 2) Validate config artifacts (recommended now)

```bash
python -m json.tool config/system_config.json >/dev/null
python -m json.tool config/system_config.schema.json >/dev/null
```

### 3) Toolchains for embedded deployment

Install one of the following:

- **ESP-IDF** (recommended for production ESP32 firmware)
  - Install ESP-IDF v5.x and export environment.
- **PlatformIO** (alternative workflow)
  - Install PlatformIO Core or VSCode PlatformIO extension.

## Flashing Firmware (after board integration is added)

Because this repo is scaffold-first, flashing requires a firmware entrypoint + board config to be added first.

### Option A: ESP-IDF

Once `CMakeLists.txt`, `main/`, and target config are added:

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

Monitor serial logs:

```bash
idf.py -p /dev/ttyUSB0 monitor
```

### Option B: PlatformIO

Once `platformio.ini` and environment are added:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Running the Firmware

After flashing a board-integrated build:

1. Boot device and confirm serial boot logs.
2. Verify config load from `/config/system_config.json` equivalent in your FS image.
3. Verify safe-mode behavior by checking boot-fail counter handling.
4. Verify trigger flow end-to-end:
   - physical button/UI/MQTT trigger
   - resolves `button_id`
   - routes through `TriggerRouter`
   - sends via `IRSender` protocol path or RAW fallback.
5. Verify MQTT + Home Assistant discovery (if enabled).

## Suggested bring-up checklist (next integration step)

- Add board pin config (`IR TX`, optional `IR RX`, OLED, buttons, sensor bus).
- Add deterministic scheduler loop with watchdog feed.
- Implement concrete services for:
  - Wi-Fi reconnect
  - MQTT publish/discovery
  - REST API / OTA
  - NVS + LittleFS/SPIFFS persistence
- Add real parser implementation in `ConfigManager` (fixed-capacity JSON parsing + schema validation policy).
- Add integration tests for `trigger(button_id) -> send()` behavior.
