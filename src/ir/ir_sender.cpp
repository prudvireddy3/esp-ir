#include "ir/ir_sender.h"

namespace esp_ir::ir {

IRSendResult IRSender::sendButton(const model::IRButton& button) {
  IRSendResult result{};

  if (button.protocol != model::IRProtocol::RAW &&
      protocol_sender_.supports(button.protocol) &&
      protocol_sender_.send(button.protocol, button.address, button.command,
                            button.repeat_behavior)) {
    result.sent = true;
    return result;
  }

  if (!button.raw_fallback.enabled || button.raw_fallback.hash.empty()) {
    return result;
  }

  auto raw = raw_store_.loadByHash(button.raw_fallback.hash.c_str());
  if (!raw.has_value()) {
    return result;
  }

  result.sent = raw_sender_.sendRaw(*raw, button.repeat_behavior);
  result.used_raw_fallback = result.sent;
  return result;
}

}  // namespace esp_ir::ir
