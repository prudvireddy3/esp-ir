# esp-ir

Production-oriented scaffold for an ESP-based IR controller firmware with:

- Hierarchical model (`Home -> Room -> Device -> Remote -> Button`)
- Single trigger path (`trigger -> button_id -> IRButton -> send()`)
- JSON config + version/migration hooks
- Boot failure tracking + safe mode entry
- Module boundaries for IR, storage, UI, network, and system reliability

See `docs/architecture.md` for architecture details.
