#include <cstdio>
#include <string>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "net/esp_idf_adapters.h"
#include "net/network_services.h"
#include "platform/board_pins.h"
#include "storage/config_manager.h"

namespace {
constexpr const char* kTag = "esp-ir";
constexpr const char* kConfigMountPath = "/config";
constexpr const char* kConfigFilePath = "/config/system_config.json";

bool mountConfigFs() {
  esp_vfs_spiffs_conf_t conf{};
  conf.base_path = kConfigMountPath;
  conf.partition_label = nullptr;
  conf.max_files = 8;
  conf.format_if_mount_failed = false;

  const esp_err_t rc = esp_vfs_spiffs_register(&conf);
  if (rc != ESP_OK) {
    ESP_LOGE(kTag, "Failed to mount config filesystem rc=%d", static_cast<int>(rc));
    return false;
  }

  size_t total_bytes = 0;
  size_t used_bytes = 0;
  const esp_err_t info_rc = esp_spiffs_info(nullptr, &total_bytes, &used_bytes);
  if (info_rc != ESP_OK) {
    ESP_LOGW(kTag, "Unable to read SPIFFS info rc=%d", static_cast<int>(info_rc));
  } else {
    ESP_LOGI(kTag, "Config FS mounted total=%u used=%u", static_cast<unsigned>(total_bytes),
             static_cast<unsigned>(used_bytes));
  }

  return true;
}

bool readConfigFile(std::string& out) {
  std::FILE* file = std::fopen(kConfigFilePath, "rb");
  if (file == nullptr) {
    ESP_LOGE(kTag, "Failed to open %s", kConfigFilePath);
    return false;
  }

  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return false;
  }

  const long file_size = std::ftell(file);
  if (file_size <= 0) {
    std::fclose(file);
    ESP_LOGE(kTag, "Config file is empty");
    return false;
  }

  if (std::fseek(file, 0, SEEK_SET) != 0) {
    std::fclose(file);
    return false;
  }

  out.resize(static_cast<size_t>(file_size));
  const size_t read_size = std::fread(out.data(), 1, out.size(), file);
  std::fclose(file);

  if (read_size != out.size()) {
    ESP_LOGE(kTag, "Config read failed read=%u expected=%u", static_cast<unsigned>(read_size),
             static_cast<unsigned>(out.size()));
    return false;
  }

  return true;
}

}  // namespace

extern "C" void app_main(void) {
  ESP_LOGI(kTag, "ESP-IR boot");

  const auto pins = esp_ir::platform::defaultBoardPins();
  ESP_LOGI(kTag, "Board pins ir_tx=%d ir_rx=%d led=%d", pins.ir_tx_gpio, pins.ir_rx_gpio,
           pins.status_led_gpio);

  if (!mountConfigFs()) {
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  std::string config_text;
  if (!readConfigFile(config_text)) {
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  esp_ir::storage::ConfigManager config_manager;
  esp_ir::model::SystemConfig config;
  if (!config_manager.migrateIfNeeded(config_text) || !config_manager.parse(config_text, config)) {
    ESP_LOGE(kTag, "Config parse failed for %s", kConfigFilePath);
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  esp_ir::net::EspMonotonicClock clock;
  esp_ir::net::EspWiFiAdapter wifi_adapter;
  esp_ir::net::EspMqttAdapter mqtt_adapter;
  esp_ir::net::EspRestApiAdapter rest_adapter;
  esp_ir::net::EspOtaAdapter ota_adapter;

  esp_ir::net::WiFiServiceImpl wifi_service(config.wifi, wifi_adapter, clock);
  esp_ir::net::MQTTServiceImpl mqtt_service(config.mqtt, config.device_id, mqtt_adapter, wifi_service);
  esp_ir::net::RestApiServiceImpl rest_service(rest_adapter);
  esp_ir::net::OTAServiceImpl ota_service(ota_adapter, config.features.ota_enabled);

  wifi_service.begin();
  rest_service.begin();
  ota_service.begin();
  mqtt_service.begin();

  while (true) {
    wifi_service.loop();
    mqtt_service.loop();
    rest_service.loop();
    ota_service.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
