#include <cstdio>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ir/ir_sender.h"
#include "net/esp_idf_adapters.h"
#include "net/network_services.h"
#include "net/trigger_router.h"
#include "platform/board_pins.h"
#include "storage/config_manager.h"
#include "system/boot_manager.h"

namespace {
constexpr const char* kTag = "esp-ir";
constexpr const char* kConfigMountPath = "/config";
constexpr const char* kConfigFilePath = "/config/system_config.json";
constexpr const char* kBootStatePath = "/config/boot_state.bin";

class FileBootStateStore final : public esp_ir::system::IBootStateStore {
 public:
  esp_ir::system::BootState load() override {
    esp_ir::system::BootState state{};
    std::FILE* fp = std::fopen(kBootStatePath, "rb");
    if (fp == nullptr) {
      return state;
    }

    std::fread(&state.failed_boots, 1, sizeof(state.failed_boots), fp);
    std::fread(&state.safe_mode, 1, sizeof(state.safe_mode), fp);

    uint8_t reason_size = 0;
    std::fread(&reason_size, 1, sizeof(reason_size), fp);
    if (reason_size > 0) {
      std::vector<char> buf(reason_size + 1U, '\0');
      std::fread(buf.data(), 1, reason_size, fp);
      state.last_crash_reason = buf.data();
    }

    std::fclose(fp);
    return state;
  }

  void save(const esp_ir::system::BootState& state) override {
    std::FILE* fp = std::fopen(kBootStatePath, "wb");
    if (fp == nullptr) {
      return;
    }

    std::fwrite(&state.failed_boots, 1, sizeof(state.failed_boots), fp);
    std::fwrite(&state.safe_mode, 1, sizeof(state.safe_mode), fp);

    uint8_t reason_size = static_cast<uint8_t>(state.last_crash_reason.size() > 200 ? 200 : state.last_crash_reason.size());
    std::fwrite(&reason_size, 1, sizeof(reason_size), fp);
    if (reason_size > 0) {
      std::fwrite(state.last_crash_reason.data(), 1, reason_size, fp);
    }

    std::fclose(fp);
  }
};

class AppButtonRepository final : public esp_ir::net::IButtonRepository {
 public:
  explicit AppButtonRepository(const esp_ir::model::SystemConfig& config) {
    for (const auto& home : config.homes) {
      for (const auto& room : home.rooms) {
        for (const auto& device : room.devices) {
          for (const auto& remote : device.remotes) {
            for (const auto& button : remote.buttons) {
              buttons_.push_back(button);
              index_[buttons_.back().button_id] = &buttons_.back();
            }
          }
        }
      }
    }
  }

  const esp_ir::model::IRButton* findById(const std::string& button_id) const override {
    const auto it = index_.find(button_id);
    return (it == index_.end()) ? nullptr : it->second;
  }

 private:
  std::vector<esp_ir::model::IRButton> buttons_;
  std::unordered_map<std::string, const esp_ir::model::IRButton*> index_;
};

class EspProtocolSender final : public esp_ir::ir::IProtocolSender {
 public:
  bool supports(esp_ir::model::IRProtocol protocol) const override {
    return protocol == esp_ir::model::IRProtocol::NEC || protocol == esp_ir::model::IRProtocol::RC5 ||
           protocol == esp_ir::model::IRProtocol::Sony;
  }

  bool send(esp_ir::model::IRProtocol protocol, uint32_t address, uint32_t command,
            esp_ir::model::RepeatBehavior) override {
    ESP_LOGI(kTag, "IR send protocol=%d address=0x%lx command=0x%lx", static_cast<int>(protocol),
             static_cast<unsigned long>(address), static_cast<unsigned long>(command));
    return true;
  }
};

class EmptyRawStore final : public esp_ir::ir::IRawStore {
 public:
  std::optional<std::vector<uint16_t>> loadByHash(const char*) override { return std::nullopt; }
};

class EspRawSender final : public esp_ir::ir::IRawSender {
 public:
  bool sendRaw(const std::vector<uint16_t>& raw_data, esp_ir::model::RepeatBehavior) override {
    ESP_LOGI(kTag, "RAW fallback send samples=%u", static_cast<unsigned>(raw_data.size()));
    return !raw_data.empty();
  }
};

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

bool readFile(const char* path, std::string& out) {
  std::FILE* file = std::fopen(path, "rb");
  if (file == nullptr) {
    ESP_LOGE(kTag, "Failed to open %s", path);
    return false;
  }

  if (std::fseek(file, 0, SEEK_END) != 0) {
    std::fclose(file);
    return false;
  }

  const long file_size = std::ftell(file);
  if (file_size <= 0) {
    std::fclose(file);
    ESP_LOGE(kTag, "File is empty: %s", path);
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
    ESP_LOGE(kTag, "Read failed path=%s read=%u expected=%u", path, static_cast<unsigned>(read_size),
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
  if (!readFile(kConfigFilePath, config_text)) {
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

  FileBootStateStore boot_store;
  esp_ir::system::BootManager boot_manager(boot_store, config.boot_fail_limit, config.safe_mode_enabled);
  const auto boot_state = boot_manager.onBootStart();
  if (boot_state.safe_mode) {
    ESP_LOGE(kTag, "SAFE MODE active (failed_boots=%u)", static_cast<unsigned>(boot_state.failed_boots));
    while (true) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  esp_ir::net::EspMonotonicClock clock;
  esp_ir::net::EspWiFiAdapter wifi_adapter;
  esp_ir::net::EspMqttAdapter mqtt_adapter;
  esp_ir::net::EspRestApiAdapter rest_adapter(kConfigFilePath);
  esp_ir::net::EspOtaAdapter ota_adapter;

  AppButtonRepository button_repo(config);
  EspProtocolSender protocol_sender;
  EmptyRawStore raw_store;
  EspRawSender raw_sender;
  esp_ir::ir::IRSender ir_sender(protocol_sender, raw_store, raw_sender);
  esp_ir::net::TriggerRouter trigger_router(button_repo, ir_sender);

  rest_adapter.setTriggerCallback([&trigger_router](const std::string& button_id) {
    return trigger_router.triggerByButtonId(button_id);
  });

  esp_ir::net::WiFiServiceImpl wifi_service(config.wifi, wifi_adapter, clock);
  esp_ir::net::MQTTServiceImpl mqtt_service(config.mqtt, config.device_id, mqtt_adapter, wifi_service);
  esp_ir::net::RestApiServiceImpl rest_service(rest_adapter);
  esp_ir::net::OTAServiceImpl ota_service(ota_adapter, config.features.ota_enabled);

  wifi_service.begin();
  rest_service.begin();
  ota_service.begin();
  mqtt_service.begin();

  ESP_LOGI(kTag, "Services started");
  boot_manager.onBootHealthy();

  while (true) {
    wifi_service.loop();
    mqtt_service.loop();
    rest_service.loop();
    ota_service.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
