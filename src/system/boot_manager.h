#pragma once

#include <cstdint>
#include <string>

namespace esp_ir::system {

struct BootState {
  uint8_t failed_boots{0};
  bool safe_mode{false};
  std::string last_crash_reason;
};

class IBootStateStore {
 public:
  virtual ~IBootStateStore() = default;
  virtual BootState load() = 0;
  virtual void save(const BootState& state) = 0;
};

class BootManager {
 public:
  BootManager(IBootStateStore& state_store, uint8_t boot_fail_limit, bool safe_mode_enabled)
      : state_store_(state_store),
        boot_fail_limit_(boot_fail_limit),
        safe_mode_enabled_(safe_mode_enabled) {}

  BootState onBootStart();
  void onBootHealthy();
  void onCrash(const std::string& crash_reason);

 private:
  IBootStateStore& state_store_;
  uint8_t boot_fail_limit_;
  bool safe_mode_enabled_;
};

}  // namespace esp_ir::system
