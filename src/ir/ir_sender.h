#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "model/ir_model.h"

namespace esp_ir::ir {

struct IRSendResult {
  bool sent{false};
  bool used_raw_fallback{false};
};

class IProtocolSender {
 public:
  virtual ~IProtocolSender() = default;
  virtual bool supports(model::IRProtocol protocol) const = 0;
  virtual bool send(model::IRProtocol protocol, uint32_t address, uint32_t command,
                    model::RepeatBehavior repeat_behavior) = 0;
};

class IRawStore {
 public:
  virtual ~IRawStore() = default;
  virtual std::optional<std::vector<uint16_t>> loadByHash(const char* hash) = 0;
};

class IRawSender {
 public:
  virtual ~IRawSender() = default;
  virtual bool sendRaw(const std::vector<uint16_t>& raw_data,
                       model::RepeatBehavior repeat_behavior) = 0;
};

class IRSender {
 public:
  IRSender(IProtocolSender& protocol_sender, IRawStore& raw_store, IRawSender& raw_sender)
      : protocol_sender_(protocol_sender), raw_store_(raw_store), raw_sender_(raw_sender) {}

  IRSendResult sendButton(const model::IRButton& button);

 private:
  IProtocolSender& protocol_sender_;
  IRawStore& raw_store_;
  IRawSender& raw_sender_;
};

}  // namespace esp_ir::ir
