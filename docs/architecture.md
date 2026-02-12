# ESP-IR Production Architecture

## Folder Structure

```text
config/
  system_config.json
  system_config.schema.json
ir/
  raw/
src/
  ir/
    ir_sender.h/.cpp
    ir_learner.h
  model/
    ir_model.h/.cpp
  storage/
    config_manager.h/.cpp
    raw_store.h
  ui/
    ui_state.h
  net/
    trigger_router.h/.cpp
    network_services.h
  system/
    boot_manager.h/.cpp
    reliability.h
```

## Core Rule: Single Trigger Path

All inputs (physical key, web UI, MQTT, voice assistant through HA) resolve to `button_id` and call a single route:

```text
trigger(source, button_id)
  -> TriggerRouter::triggerByButtonId(button_id)
  -> IRepository::findById(button_id)
  -> IRSender::sendButton(IRButton)
  -> protocol send OR raw fallback send
```

No source-specific IR code paths are allowed.

## IR Abstraction

`IRButton` is the execution unit and always contains:

- `protocol`
- `address`
- `command`
- `raw_fallback` (hash into `/ir/raw/*.bin`)
- `repeat_behavior`

This keeps app logic independent from pulse-level details.

## Config Lifecycle

1. Load `/config/system_config.json`.
2. Validate required fields against schema.
3. Run `migrateIfNeeded()` using `schema_version`.
4. Materialize in-memory models for runtime lookup.

## Boot + Safe Mode

- On each boot, increment failed-boot counter in persistent store.
- If `failed_boots >= boot_fail_limit` and safe mode is enabled, activate safe mode.
- Mark boot healthy only when scheduler and critical services initialize.
- On crash, persist reset reason for next startup diagnostics.

## Reliability Baseline (must stay enabled)

- Watchdog feed in the main scheduler tick.
- Crash reason persistence.
- Boot failure counter + safe mode.
- OTA service prepared for rollback state checks.
- Wi-Fi reconnect timer (non-blocking).
- MQTT offline buffering behavior (with offline mode support).
- IR send retry logic from config (`ir_retry_count`, `ir_retry_delay_ms`).
- NTP sync from config-defined servers.
