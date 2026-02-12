#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace esp_ir::model {

enum class IRProtocol {
  NEC,
  RC5,
  Sony,
  RAW,
  Unknown,
};

enum class RepeatBehavior {
  None,
  Hold,
  Timed,
};

struct RawFallbackRef {
  bool enabled{false};
  std::string hash;
};

struct IRButton {
  std::string button_id;
  std::string label;
  IRProtocol protocol{IRProtocol::Unknown};
  uint32_t address{0};
  uint32_t command{0};
  RepeatBehavior repeat_behavior{RepeatBehavior::None};
  RawFallbackRef raw_fallback;
};

struct Remote {
  std::string remote_id;
  std::string name;
  std::vector<IRButton> buttons;
};

struct Device {
  std::string device_id;
  std::string name;
  std::string type;
  std::string manufacturer;
  std::string model;
  std::vector<Remote> remotes;
};

struct Room {
  std::string room_id;
  std::string name;
  std::vector<Device> devices;
};

struct Home {
  std::string home_id;
  std::string name;
  std::vector<Room> rooms;
};

struct MQTTConfig {
  bool enabled{true};
  std::string broker;
  uint16_t port{1883};
  std::string username;
  std::string password;
  std::string base_topic;
  bool ha_discovery{true};
  bool retain{true};
};

struct WiFiConfig {
  std::string ssid;
  std::string password;
  bool ap_disable_after_setup{true};
  uint16_t reconnect_interval_sec{10};
};

struct FeatureFlags {
  bool offline_mode{true};
  uint8_t ir_retry_count{2};
  uint16_t ir_retry_delay_ms{150};
  bool crash_logging{true};
  bool ota_enabled{true};
};

struct SystemConfig {
  uint16_t schema_version{1};
  std::string device_id;
  std::string timezone;
  uint8_t boot_fail_limit{3};
  bool safe_mode_enabled{true};
  WiFiConfig wifi;
  MQTTConfig mqtt;
  FeatureFlags features;
  std::vector<Home> homes;
};

}  // namespace esp_ir::model
