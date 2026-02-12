#pragma once

#include <string>

namespace esp_ir::ui {

struct UIStatus {
  std::string current_home;
  std::string current_room;
  std::string current_device;
  std::string selected_button_label;
  float temperature_c{0.0f};
  float humidity_pct{0.0f};
  bool wifi_connected{false};
  bool mqtt_connected{false};
};

enum class ScreenMode {
  Boot,
  Normal,
  Error,
  SafeMode,
};

}  // namespace esp_ir::ui
