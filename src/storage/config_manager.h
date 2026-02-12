#pragma once

#include <string>

#include "model/ir_model.h"

namespace esp_ir::storage {

struct ValidationResult {
  bool ok{false};
  std::string error;
};

class ConfigManager {
 public:
  static constexpr uint16_t kCurrentSchemaVersion = 1;

  ValidationResult validateJsonText(const std::string& json_text) const;
  bool migrateIfNeeded(std::string& json_text) const;
  bool parse(const std::string& json_text, model::SystemConfig& out) const;
};

}  // namespace esp_ir::storage
