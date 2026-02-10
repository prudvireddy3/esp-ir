#pragma once

#include <optional>
#include <string>
#include <vector>

namespace esp_ir::storage {

class RawStore {
 public:
  virtual ~RawStore() = default;
  virtual bool saveCompressed(const std::string& hash, const std::vector<uint16_t>& raw_data) = 0;
  virtual std::optional<std::vector<uint16_t>> load(const std::string& hash) const = 0;
};

}  // namespace esp_ir::storage
