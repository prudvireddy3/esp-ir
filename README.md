# esp-ir

Production-oriented **ESP-IDF firmware scaffold** for an ESP-based IR controller.

Designed around strict architectural boundaries, deterministic behavior, and a single IR execution path. This is not a demo project — it is structured for real firmware deployment.

---

# Core Design Principles

- Hierarchical model  
  `Home → Room → Device → Remote → Button`

- Single trigger path  
  `trigger → button_id → IRButton → send()`

- Config-driven behavior (no hardcoded devices)

- Protocol-first IR send with RAW fallback

- Boot failure tracking + safe mode entry

- Clear module boundaries:
  - `ir/`
  - `model/`
  - `storage/`
  - `net/`
  - `ui/`
  - `system/`

See `docs/architecture.md` for detailed architecture constraints.

---

# ESP-IDF Project Layout

This repository includes a proper ESP-IDF build target:

- Root `CMakeLists.txt`
- `main/app_main.cpp` (firmware entrypoint)
- `components/esp_ir/CMakeLists.txt` (builds scaffold sources with C++17)

Target: ESP-IDF v5.x

---

# Installation

## 1) Clone

```bash
git clone <your-repo-url> esp-ir
cd esp-ir
```

---

## 2) Validate Config Files

```bash
python -m json.tool config/system_config.json >/dev/null
python -m json.tool config/system_config.schema.json >/dev/null
```

---

# ESP-IDF Environment Setup (Required Every New Terminal)

You must export ESP-IDF into your shell before using `idf.py`.

## macOS / Linux

```bash
. $HOME/esp-idf/export.sh
```

If installed elsewhere:

```bash
. /path/to/esp-idf/export.sh
```

## Verify Environment

```bash
which idf.py
idf.py --version
```

If `idf.py` is not found, the environment is not active.

You must run the export script in every new terminal session.

---

# Target Selection (First Time Only)

```bash
idf.py set-target esp32
```

For ESP32-S3 or C3:

```bash
idf.py set-target esp32s3
```

Changing targets later requires:

```bash
idf.py fullclean
idf.py set-target <chip>
```

---

# Project Configuration

```bash
idf.py menuconfig
```

Verify:

- Partition table supports OTA (`factory` + `ota_0`)
- Flash size matches hardware
- LittleFS or SPIFFS enabled (if filesystem used)
- Wi-Fi enabled
- Task watchdog enabled
- Correct serial port selected

Save and exit.

---

# Build

```bash
idf.py build
```

If CMake needs refresh:

```bash
idf.py reconfigure
```

For a clean rebuild:

```bash
idf.py fullclean
idf.py build
```

---

# Flash

Specify port if needed:

```bash
idf.py -p /dev/ttyUSB0 flash
```

Or auto-detect:

```bash
idf.py flash
```

---

# Monitor

```bash
idf.py monitor
```

Combined:

```bash
idf.py flash monitor
```

Exit monitor:

```
Ctrl + ]
```

---

# Filesystem & Config Requirements

The firmware expects:

```
/config/system_config.json
```

to exist in the mounted filesystem at runtime.

You must:

- Define a `data` partition in your partition table
- Enable LittleFS or SPIFFS
- Ensure config files are included in the filesystem image

Example partition entry:

```
data, littlefs, ...
```

Without a filesystem partition, config loading will fail.

This firmware is config-driven. No config = no runtime behavior.

---

# Typical Development Loop

```bash
. $HOME/esp-idf/export.sh
idf.py build
idf.py flash monitor
```

If modifying:

- Only `.cpp/.h` files → incremental build is automatic
- `CMakeLists.txt` or component structure → run `idf.py reconfigure`

---

# Running / Bring-Up Checklist

After flashing:

1. Confirm boot log from `app_main`.
2. Confirm boot counter increments.
3. Verify safe-mode activation after configured failure threshold.
4. Verify config file loads successfully.
5. Validate single trigger path:
   - physical button / web / MQTT trigger
   - resolve `button_id`
   - `TriggerRouter::triggerByButtonId()`
   - `IRSender::sendButton()`
   - protocol path OR RAW fallback
6. Confirm MQTT discovery entities appear in Home Assistant (once MQTT integration is enabled).

---

# Current Implementation Status

This repository is buildable and architecturally structured, but not fully hardware-integrated.

## Implemented

- ESP-IDF project structure
- Single trigger path enforcement
- Protocol-first IR send + RAW fallback
- Boot failure counter + safe mode flagging
- Config validation + migration hooks (scaffold level)

## Pending Production Hardening

- Deterministic JSON parsing (replace string-based validation)
- Filesystem mounting + config materialization at boot
- Concrete Wi-Fi / MQTT / REST / OTA services
- Watchdog feed wiring
- Safe-mode runtime branching
- Production partition table
- Board-specific pin mapping

This is a firmware scaffold ready for hardening — not a finished product.
