#include "storage/config_manager.h"

namespace esp_ir::storage {

ValidationResult ConfigManager::validateJsonText(const std::string& json_text) const {
  if (json_text.find("\"schema_version\"") == std::string::npos) {
    return {.ok = false, .error = "missing schema_version"};
  }

  if (json_text.find("\"homes\"") == std::string::npos) {
    return {.ok = false, .error = "missing homes hierarchy"};
  }

  return {.ok = true, .error = ""};
}

bool ConfigManager::migrateIfNeeded(std::string& json_text) const {
  const auto has_v0 = json_text.find("\"schema_version\":0") != std::string::npos ||
                      json_text.find("\"schema_version\": 0") != std::string::npos;

  if (!has_v0) {
    return true;
  }

  const auto key_pos = json_text.find("\"schema_version\"");
  if (key_pos == std::string::npos) {
    return false;
  }

  const auto value_pos = json_text.find('0', key_pos);
  if (value_pos == std::string::npos) {
    return false;
  }

  json_text[value_pos] = '1';
  return true;
}

bool ConfigManager::parse(const std::string& json_text, model::SystemConfig& out) const {
  // Production firmware should parse with a deterministic JSON parser
  // (for example ArduinoJson with preallocated document capacity).
  // This scaffold sets required defaults and leaves full parsing to integration.
  auto validation = validateJsonText(json_text);
  if (!validation.ok) {
    return false;
  }

  out.schema_version = kCurrentSchemaVersion;
  return true;
}

}  // namespace esp_ir::storage
