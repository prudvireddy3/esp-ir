#!/usr/bin/env python3
import copy
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = ROOT / "config" / "system_config.json"

REQUIRED_TOP = ["schema_version", "system", "network", "mqtt", "homes"]


def should_pass_like_parser(doc: dict) -> bool:
    try:
        if any(k not in doc for k in REQUIRED_TOP):
            return False
        if doc["schema_version"] != 1:
            return False
        system = doc["system"]
        wifi = doc["network"]["wifi"]
        mqtt = doc["mqtt"]
        _ = system["device_id"]
        _ = system["timezone"]
        if not isinstance(system["boot_fail_limit"], int):
            return False
        if not isinstance(system["safe_mode_enabled"], bool):
            return False
        _ = wifi["ssid"]
        _ = wifi["password"]
        if not isinstance(wifi["reconnect_interval_sec"], int):
            return False
        _ = mqtt["broker"]
        if not isinstance(mqtt["port"], int):
            return False
        _ = mqtt["base_topic"]
        if not isinstance(mqtt["enabled"], bool) or not isinstance(mqtt["ha_discovery"], bool) or not isinstance(mqtt["retain"], bool):
            return False
        assert isinstance(doc["homes"], list)
        allowed_protocols = {"NEC", "RC5", "Sony", "RAW"}
        for home in doc["homes"]:
            for hk in ["home_id", "name", "rooms"]:
                if hk not in home:
                    return False
            for room in home["rooms"]:
                for rk in ["room_id", "name", "devices"]:
                    if rk not in room:
                        return False
                for device in room["devices"]:
                    for dk in ["device_id", "name", "remotes"]:
                        if dk not in device:
                            return False
                    for remote in device["remotes"]:
                        for remk in ["remote_id", "name", "buttons"]:
                            if remk not in remote:
                                return False
                        for button in remote["buttons"]:
                            for bk in ["button_id", "label", "protocol", "address", "command", "repeat_behavior"]:
                                if bk not in button:
                                    return False
                            if button["protocol"] not in allowed_protocols:
                                return False
        return True
    except Exception:
        return False


def main() -> int:
    base = json.loads(CFG.read_text())

    assert should_pass_like_parser(base), "baseline config must pass"

    negatives = []
    for key in REQUIRED_TOP:
        bad = copy.deepcopy(base)
        bad.pop(key, None)
        negatives.append((f"missing_{key}", bad))

    bad_ver = copy.deepcopy(base)
    bad_ver["schema_version"] = 2
    negatives.append(("unsupported_schema", bad_ver))

    bad_button = copy.deepcopy(base)
    bad_button["homes"][0]["rooms"][0]["devices"][0]["remotes"][0]["buttons"][0]["protocol"] = "INVALID"
    negatives.append(("invalid_protocol", bad_button))

    for name, doc in negatives:
        if should_pass_like_parser(doc):
            print(f"FAIL: expected invalid: {name}")
            return 1

    rng = random.Random(1337)
    for i in range(100):
        m = copy.deepcopy(base)
        choice = rng.choice(["drop_top", "flip_type", "clear_array"])
        if choice == "drop_top":
            m.pop(rng.choice(REQUIRED_TOP), None)
        elif choice == "flip_type":
            m["mqtt"]["port"] = "1883"
        else:
            m["homes"] = {}

        if should_pass_like_parser(m):
            print(f"FAIL: fuzz mutation should not pass at iteration {i}")
            return 1

    print("OK: config parser negative/fuzz cases behaved as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
