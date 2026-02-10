#include "net/trigger_router.h"

namespace esp_ir::net {

bool TriggerRouter::triggerByButtonId(const std::string& button_id) {
  const auto* button = buttons_.findById(button_id);
  if (button == nullptr) {
    return false;
  }

  auto result = ir_sender_.sendButton(*button);
  return result.sent;
}

}  // namespace esp_ir::net
