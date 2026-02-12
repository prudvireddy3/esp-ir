#pragma once

#include <string>

#include "ir/ir_sender.h"
#include "model/ir_model.h"

namespace esp_ir::net {

class IButtonRepository {
 public:
  virtual ~IButtonRepository() = default;
  virtual const model::IRButton* findById(const std::string& button_id) const = 0;
};

class TriggerRouter {
 public:
  TriggerRouter(const IButtonRepository& buttons, ir::IRSender& ir_sender)
      : buttons_(buttons), ir_sender_(ir_sender) {}

  bool triggerByButtonId(const std::string& button_id);

 private:
  const IButtonRepository& buttons_;
  ir::IRSender& ir_sender_;
};

}  // namespace esp_ir::net
