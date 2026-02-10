#pragma once

#include <optional>
#include <vector>

#include "model/ir_model.h"

namespace esp_ir::ir {

struct LearnedIRSignal {
  model::IRProtocol protocol{model::IRProtocol::Unknown};
  uint32_t address{0};
  uint32_t command{0};
  std::vector<uint16_t> raw_data;
};

class IIRLearner {
 public:
  virtual ~IIRLearner() = default;
  virtual std::optional<LearnedIRSignal> learnOnce(uint32_t timeout_ms) = 0;
};

}  // namespace esp_ir::ir
