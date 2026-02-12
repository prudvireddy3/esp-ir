# esp-ir

Production-oriented ESP-IDF firmware scaffold for an ESP-based IR controller with:

- Hierarchical model (`Home -> Room -> Device -> Remote -> Button`)
- Single trigger path (`trigger -> button_id -> IRButton -> send()`)
- JSON config + version/migration hooks
- Boot failure tracking + safe mode entry
- Module boundaries for IR, storage, UI, network, and system reliability

See `docs/architecture.md` for architecture details.

## ESP-IDF Project Layout

This repository now includes an ESP-IDF build target:

- Root `CMakeLists.txt`
- `main/app_main.cpp`
- `components/esp_ir/CMakeLists.txt` that compiles core scaffold sources

## Installation

### 1) Clone

```bash
git clone <your-repo-url> esp-ir
cd esp-ir
```

### 2) Validate config artifacts

```bash
python -m json.tool config/system_config.json >/dev/null
python -m json.tool config/system_config.schema.json >/dev/null
```

### 3) Install ESP-IDF

Install ESP-IDF v5.x (per Espressif docs).

## ESP-IDF Environment Setup (Required Every New Terminal)

ESP-IDF must be exported into your shell environment before running any `idf.py` commands.

### macOS / Linux

```bash
. $HOME/esp-idf/export.sh
```

If installed elsewhere:

```bash
. /path/to/esp-idf/export.sh
```

### Verify Environment

```bash
which idf.py
idf.py --version
```

If `idf.py` is not found, the environment is not exported.

You must run the export script in every new terminal session.

## Target Selection (First Time Only)

```bash
idf.py set-target esp32
```

If using ESP32-S3 or C3, adjust accordingly.

Changing targets later requires:

```bash
idf.py fullclean
idf.py set-target <chip>
```

## Project Configuration (menuconfig)

```bash
idf.py menuconfig
```

Verify:

- Partition table supports OTA (factory + `ota_0`)
- Flash size matches hardware
- LittleFS (or SPIFFS) enabled if used
- Wi-Fi enabled
- Watchdog enabled

Save and exit.

## Build

```bash
idf.py build
```

If CMake cache issues occur:

```bash
idf.py reconfigure
```

For a clean rebuild:

```bash
idf.py fullclean
idf.py build
```

## Flash

```bash
idf.py -p /dev/ttyUSB0 flash
```

Or auto-detect:

```bash
idf.py flash
```

## Monitor

```bash
idf.py monitor
```

Or combined:

```bash
idf.py flash monitor
```

Exit monitor:

```text
Ctrl + ]
```

## Filesystem Image (LittleFS/SPIFFS)

If `/config/system_config.json` must exist at runtime, you must:

- Create FS image from `/config`
- Flash filesystem partition

Example (LittleFS component required):

```bash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

If using a data partition, ensure partition table includes:

```text
data, littlefs, ...
```

And that config files are included via component or data folder.

⚠ Without a filesystem partition, config loading will fail at runtime.

## Typical Development Loop

```bash
. $HOME/esp-idf/export.sh
idf.py build
idf.py flash monitor
```

If modifying only C++ sources, incremental build is automatic.

If modifying `CMakeLists.txt` or components:

```bash
idf.py reconfigure
idf.py build
```

## Running / Bring-up Checklist

After flashing:

1. Confirm boot log from `app_main`.
2. Verify runtime loads `/config/system_config.json` from mounted SPIFFS (no built-in JSON fallback).
3. Verify safe-mode behavior from boot-fail counter logic.
4. Verify end-to-end trigger path:
   - physical button / web / MQTT trigger
   - resolve `button_id`
   - `TriggerRouter::triggerByButtonId()`
   - `IRSender::sendButton()` protocol path or RAW fallback.
5. Verify MQTT discovery entities in Home Assistant (when MQTT service implementation is integrated).

## Current Scope

This project is buildable with ESP-IDF, but still intentionally scaffolded for hardware integrations:

- Wi-Fi/MQTT/REST/OTA services are wired to concrete ESP-IDF adapters (`esp_wifi`, `esp-mqtt`, `esp_http_server`, OTA service hook).
- Board pin defaults are centralized in `src/platform/board_pins.h` and logged at boot; update these values for your PCB.
- `ConfigManager` now performs deterministic cJSON parsing, required-field validation, and hierarchy decoding (homes/rooms/devices/remotes/buttons).


## REST + OTA Runtime Endpoints

Implemented endpoints:

- `GET /api/v1/health`
- `GET /api/v1/config`
- `PUT /api/v1/config`
- `GET /api/v1/homes`
- `POST /api/v1/learn/start`
- `POST /api/v1/learn/stop`
- `POST /api/v1/trigger` (body: `{"button_id":"..."}`)
- `POST /api/v1/ota` (body: `{"url":"https://...bin"}`)

OTA runs through `esp_https_ota` in the main service loop and reboots automatically on success.
