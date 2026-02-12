#include "storage/config_manager.h"

#include <cctype>
#include <cstdlib>
#include <utility>

#include "cJSON.h"

namespace esp_ir::storage {
namespace {

const cJSON* requireObjectMember(const cJSON* parent, const char* key) {
  if (parent == nullptr || !cJSON_IsObject(parent)) {
    return nullptr;
  }

  const cJSON* node = cJSON_GetObjectItemCaseSensitive(parent, key);
  if (node == nullptr || cJSON_IsNull(node)) {
    return nullptr;
  }

  return node;
}

bool parseBool(const cJSON* parent, const char* key, bool& out) {
  const cJSON* node = requireObjectMember(parent, key);
  if (node == nullptr || !cJSON_IsBool(node)) {
    return false;
  }

  out = cJSON_IsTrue(node);
  return true;
}

bool parseString(const cJSON* parent, const char* key, std::string& out) {
  const cJSON* node = requireObjectMember(parent, key);
  if (node == nullptr || !cJSON_IsString(node) || node->valuestring == nullptr) {
    return false;
  }

  out = node->valuestring;
  return true;
}

bool parseUInt16(const cJSON* parent, const char* key, uint16_t& out) {
  const cJSON* node = requireObjectMember(parent, key);
  if (node == nullptr || !cJSON_IsNumber(node) || node->valuedouble < 0 || node->valuedouble > 65535) {
    return false;
  }

  out = static_cast<uint16_t>(node->valueint);
  return true;
}

bool parseUInt8(const cJSON* parent, const char* key, uint8_t& out) {
  const cJSON* node = requireObjectMember(parent, key);
  if (node == nullptr || !cJSON_IsNumber(node) || node->valuedouble < 0 || node->valuedouble > 255) {
    return false;
  }

  out = static_cast<uint8_t>(node->valueint);
  return true;
}

bool parseHexOrInt(const cJSON* parent, const char* key, uint32_t& out) {
  const cJSON* node = requireObjectMember(parent, key);
  if (node == nullptr) {
    return false;
  }

  if (cJSON_IsNumber(node)) {
    if (node->valuedouble < 0 || node->valuedouble > 4294967295.0) {
      return false;
    }
    out = static_cast<uint32_t>(node->valuedouble);
    return true;
  }

  if (!cJSON_IsString(node) || node->valuestring == nullptr) {
    return false;
  }

  char* end_ptr = nullptr;
  const unsigned long parsed = std::strtoul(node->valuestring, &end_ptr, 0);
  if (end_ptr == node->valuestring || *end_ptr != '\0') {
    return false;
  }

  out = static_cast<uint32_t>(parsed);
  return true;
}

model::IRProtocol parseProtocol(const std::string& protocol) {
  if (protocol == "NEC") {
    return model::IRProtocol::NEC;
  }
  if (protocol == "RC5") {
    return model::IRProtocol::RC5;
  }
  if (protocol == "Sony") {
    return model::IRProtocol::Sony;
  }
  if (protocol == "RAW") {
    return model::IRProtocol::RAW;
  }

  return model::IRProtocol::Unknown;
}

bool parseRepeat(const std::string& repeat, model::RepeatBehavior& out) {
  if (repeat == "none") {
    out = model::RepeatBehavior::None;
    return true;
  }
  if (repeat == "hold") {
    out = model::RepeatBehavior::Hold;
    return true;
  }
  if (repeat == "timed") {
    out = model::RepeatBehavior::Timed;
    return true;
  }

  return false;
}

bool parseButton(const cJSON* button_json, model::IRButton& button) {
  if (!parseString(button_json, "button_id", button.button_id) || button.button_id.empty() ||
      !parseString(button_json, "label", button.label)) {
    return false;
  }

  std::string protocol_string;
  if (!parseString(button_json, "protocol", protocol_string)) {
    return false;
  }
  button.protocol = parseProtocol(protocol_string);
  if (button.protocol == model::IRProtocol::Unknown) {
    return false;
  }

  if (!parseHexOrInt(button_json, "address", button.address) ||
      !parseHexOrInt(button_json, "command", button.command)) {
    return false;
  }

  std::string repeat;
  if (!parseString(button_json, "repeat_behavior", repeat) ||
      !parseRepeat(repeat, button.repeat_behavior)) {
    return false;
  }

  const cJSON* raw_fallback = requireObjectMember(button_json, "raw_fallback");
  if (raw_fallback != nullptr) {
    if (!parseBool(raw_fallback, "enabled", button.raw_fallback.enabled) ||
        !parseString(raw_fallback, "hash", button.raw_fallback.hash)) {
      return false;
    }
  }

  return true;
}

bool parseRemote(const cJSON* remote_json, model::Remote& remote) {
  if (!parseString(remote_json, "remote_id", remote.remote_id) || !parseString(remote_json, "name", remote.name)) {
    return false;
  }

  const cJSON* buttons = requireObjectMember(remote_json, "buttons");
  if (buttons == nullptr || !cJSON_IsArray(buttons)) {
    return false;
  }

  cJSON* button_json = nullptr;
  cJSON_ArrayForEach(button_json, buttons) {
    model::IRButton button;
    if (!parseButton(button_json, button)) {
      return false;
    }
    remote.buttons.push_back(std::move(button));
  }

  return true;
}

bool parseDevice(const cJSON* device_json, model::Device& device) {
  if (!parseString(device_json, "device_id", device.device_id) || !parseString(device_json, "name", device.name)) {
    return false;
  }

  (void)parseString(device_json, "type", device.type);
  (void)parseString(device_json, "manufacturer", device.manufacturer);
  (void)parseString(device_json, "model", device.model);

  const cJSON* remotes = requireObjectMember(device_json, "remotes");
  if (remotes == nullptr || !cJSON_IsArray(remotes)) {
    return false;
  }

  cJSON* remote_json = nullptr;
  cJSON_ArrayForEach(remote_json, remotes) {
    model::Remote remote;
    if (!parseRemote(remote_json, remote)) {
      return false;
    }
    device.remotes.push_back(std::move(remote));
  }

  return true;
}

bool parseRoom(const cJSON* room_json, model::Room& room) {
  if (!parseString(room_json, "room_id", room.room_id) || !parseString(room_json, "name", room.name)) {
    return false;
  }

  const cJSON* devices = requireObjectMember(room_json, "devices");
  if (devices == nullptr || !cJSON_IsArray(devices)) {
    return false;
  }

  cJSON* device_json = nullptr;
  cJSON_ArrayForEach(device_json, devices) {
    model::Device device;
    if (!parseDevice(device_json, device)) {
      return false;
    }
    room.devices.push_back(std::move(device));
  }

  return true;
}

bool parseHome(const cJSON* home_json, model::Home& home) {
  if (!parseString(home_json, "home_id", home.home_id) || !parseString(home_json, "name", home.name)) {
    return false;
  }

  const cJSON* rooms = requireObjectMember(home_json, "rooms");
  if (rooms == nullptr || !cJSON_IsArray(rooms)) {
    return false;
  }

  cJSON* room_json = nullptr;
  cJSON_ArrayForEach(room_json, rooms) {
    model::Room room;
    if (!parseRoom(room_json, room)) {
      return false;
    }
    home.rooms.push_back(std::move(room));
  }

  return true;
}

bool parseSchemaVersion(const cJSON* root, uint16_t& schema_version) {
  const cJSON* node = requireObjectMember(root, "schema_version");
  if (node == nullptr || !cJSON_IsNumber(node) || node->valuedouble < 0 || node->valuedouble > 65535) {
    return false;
  }

  schema_version = static_cast<uint16_t>(node->valueint);
  return true;
}

}  // namespace

ValidationResult ConfigManager::validateJsonText(const std::string& json_text) const {
  cJSON* root = cJSON_Parse(json_text.c_str());
  if (root == nullptr) {
    return {.ok = false, .error = "invalid JSON"};
  }

  uint16_t schema_version = 0;
  const bool has_schema = parseSchemaVersion(root, schema_version);
  const cJSON* system = requireObjectMember(root, "system");
  const cJSON* network = requireObjectMember(root, "network");
  const cJSON* mqtt = requireObjectMember(root, "mqtt");
  const cJSON* homes = requireObjectMember(root, "homes");

  ValidationResult result{.ok = true, .error = ""};
  if (!has_schema) {
    result = {.ok = false, .error = "missing or invalid schema_version"};
  } else if (system == nullptr || !cJSON_IsObject(system)) {
    result = {.ok = false, .error = "missing system"};
  } else if (network == nullptr || !cJSON_IsObject(network)) {
    result = {.ok = false, .error = "missing network"};
  } else if (mqtt == nullptr || !cJSON_IsObject(mqtt)) {
    result = {.ok = false, .error = "missing mqtt"};
  } else if (homes == nullptr || !cJSON_IsArray(homes)) {
    result = {.ok = false, .error = "missing homes"};
  }

  cJSON_Delete(root);
  return result;
}

bool ConfigManager::migrateIfNeeded(std::string& json_text) const {
  cJSON* root = cJSON_Parse(json_text.c_str());
  if (root == nullptr) {
    return false;
  }

  uint16_t schema_version = 0;
  if (!parseSchemaVersion(root, schema_version)) {
    cJSON_Delete(root);
    return false;
  }

  if (schema_version == kCurrentSchemaVersion) {
    cJSON_Delete(root);
    return true;
  }

  if (schema_version != 0) {
    cJSON_Delete(root);
    return false;
  }

  cJSON_ReplaceItemInObjectCaseSensitive(root, "schema_version", cJSON_CreateNumber(kCurrentSchemaVersion));
  char* serialized = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (serialized == nullptr) {
    return false;
  }

  json_text.assign(serialized);
  cJSON_free(serialized);
  return true;
}

bool ConfigManager::parse(const std::string& json_text, model::SystemConfig& out) const {
  cJSON* root = cJSON_Parse(json_text.c_str());
  if (root == nullptr) {
    return false;
  }

  model::SystemConfig parsed{};

  if (!parseSchemaVersion(root, parsed.schema_version) || parsed.schema_version != kCurrentSchemaVersion) {
    cJSON_Delete(root);
    return false;
  }

  const cJSON* system = requireObjectMember(root, "system");
  const cJSON* network = requireObjectMember(root, "network");
  const cJSON* wifi = (network == nullptr) ? nullptr : requireObjectMember(network, "wifi");
  const cJSON* mqtt = requireObjectMember(root, "mqtt");
  const cJSON* features = requireObjectMember(root, "features");
  const cJSON* homes = requireObjectMember(root, "homes");

  if (system == nullptr || wifi == nullptr || mqtt == nullptr || homes == nullptr || !cJSON_IsArray(homes)) {
    cJSON_Delete(root);
    return false;
  }

  if (!parseString(system, "device_id", parsed.device_id) || !parseString(system, "timezone", parsed.timezone) ||
      !parseUInt8(system, "boot_fail_limit", parsed.boot_fail_limit) ||
      !parseBool(system, "safe_mode_enabled", parsed.safe_mode_enabled) ||
      !parseString(wifi, "ssid", parsed.wifi.ssid) || !parseString(wifi, "password", parsed.wifi.password) ||
      !parseBool(wifi, "ap_disable_after_setup", parsed.wifi.ap_disable_after_setup) ||
      !parseUInt16(wifi, "reconnect_interval_sec", parsed.wifi.reconnect_interval_sec) ||
      !parseBool(mqtt, "enabled", parsed.mqtt.enabled) || !parseString(mqtt, "broker", parsed.mqtt.broker) ||
      !parseUInt16(mqtt, "port", parsed.mqtt.port) || !parseString(mqtt, "base_topic", parsed.mqtt.base_topic) ||
      !parseBool(mqtt, "ha_discovery", parsed.mqtt.ha_discovery) ||
      !parseBool(mqtt, "retain", parsed.mqtt.retain)) {
    cJSON_Delete(root);
    return false;
  }

  (void)parseString(mqtt, "username", parsed.mqtt.username);
  (void)parseString(mqtt, "password", parsed.mqtt.password);

  if (features != nullptr) {
    (void)parseBool(features, "offline_mode", parsed.features.offline_mode);
    (void)parseUInt8(features, "ir_retry_count", parsed.features.ir_retry_count);
    (void)parseUInt16(features, "ir_retry_delay_ms", parsed.features.ir_retry_delay_ms);
    (void)parseBool(features, "crash_logging", parsed.features.crash_logging);
    (void)parseBool(features, "ota_enabled", parsed.features.ota_enabled);
  }

  cJSON* home_json = nullptr;
  cJSON_ArrayForEach(home_json, homes) {
    model::Home home;
    if (!parseHome(home_json, home)) {
      cJSON_Delete(root);
      return false;
    }

    parsed.homes.push_back(std::move(home));
  }

  out = std::move(parsed);
  cJSON_Delete(root);
  return true;
}

}  // namespace esp_ir::storage
