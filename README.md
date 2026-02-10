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

Run this first in the terminal:

```bash
. $HOME/esp-idf/export.sh
```

Verify (optional):

```bash
which idf.py
```

Then run all `idf.py` commands from this repo root.

## Build

```bash
idf.py set-target esp32
idf.py build
```

## Flash

```bash
idf.py flash monitor
```

## Running / Bring-up Checklist

After flashing:

1. Confirm boot log from `app_main`.
2. Verify config load path for `/config/system_config.json` equivalent in FS image.
3. Verify safe-mode behavior from boot-fail counter logic.
4. Verify end-to-end trigger path:
   - physical button / web / MQTT trigger
   - resolve `button_id`
   - `TriggerRouter::triggerByButtonId()`
   - `IRSender::sendButton()` protocol path or RAW fallback.
5. Verify MQTT discovery entities in Home Assistant (when MQTT service implementation is integrated).

## Current Scope

This project is buildable with ESP-IDF, but still intentionally scaffolded for hardware integrations:

- Concrete Wi-Fi/MQTT/REST/OTA implementations are interface-first.
- Board pin mapping and driver wiring are still required.
- `ConfigManager` parser should be upgraded to deterministic full JSON parsing for production.
