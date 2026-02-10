#include "system/boot_manager.h"

namespace esp_ir::system {

BootState BootManager::onBootStart() {
  BootState state = state_store_.load();

  if (state.failed_boots < 255) {
    state.failed_boots += 1;
  }

  if (safe_mode_enabled_ && state.failed_boots >= boot_fail_limit_) {
    state.safe_mode = true;
  }

  state_store_.save(state);
  return state;
}

void BootManager::onBootHealthy() {
  BootState state = state_store_.load();
  state.failed_boots = 0;
  state.safe_mode = false;
  state_store_.save(state);
}

void BootManager::onCrash(const std::string& crash_reason) {
  BootState state = state_store_.load();
  state.last_crash_reason = crash_reason;
  state_store_.save(state);
}

}  // namespace esp_ir::system
