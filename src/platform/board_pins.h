#pragma once

#include <cstdint>

namespace esp_ir::platform {

struct BoardPins {
  int ir_tx_gpio{4};
  int ir_rx_gpio{5};
  int status_led_gpio{2};
  int i2c_sda_gpio{21};
  int i2c_scl_gpio{22};
  int user_btn_up_gpio{32};
  int user_btn_down_gpio{33};
  int user_btn_left_gpio{25};
  int user_btn_right_gpio{26};
  int user_btn_select_gpio{27};
};

BoardPins defaultBoardPins();

}  // namespace esp_ir::platform
