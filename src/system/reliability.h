#pragma once

#include <cstdint>

namespace esp_ir::system {

class WatchdogService {
 public:
  virtual ~WatchdogService() = default;
  virtual void init(uint32_t timeout_ms) = 0;
  virtual void feed() = 0;
};

class CrashLogger {
 public:
  virtual ~CrashLogger() = default;
  virtual void persistCrashReason(const char* reason) = 0;
  virtual const char* loadCrashReason() = 0;
};

class SafeModeService {
 public:
  virtual ~SafeModeService() = default;
  virtual bool active() const = 0;
  virtual void enter() = 0;
};

}  // namespace esp_ir::system
