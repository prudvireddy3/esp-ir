idf_component_register(
  SRCS
    "../../src/model/ir_model.cpp"
    "../../src/ir/ir_sender.cpp"
    "../../src/net/trigger_router.cpp"
    "../../src/net/network_services.cpp"
    "../../src/net/esp_idf_adapters.cpp"
    "../../src/storage/config_manager.cpp"
    "../../src/system/boot_manager.cpp"
    "../../src/platform/board_pins.cpp"
  INCLUDE_DIRS
    "../../src"
  REQUIRES
    esp_wifi
    esp_event
    esp_netif
    mqtt
    esp_http_server
    esp_timer
    json
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
