#include "net/esp_idf_adapters.h"

#ifdef ESP_PLATFORM

#include <cstring>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

namespace esp_ir::net {
namespace {
constexpr const char* kTag = "esp-idf-net";
}

uint64_t EspMonotonicClock::nowMs() const { return static_cast<uint64_t>(esp_timer_get_time() / 1000ULL); }

bool EspWiFiAdapter::startIfNeeded() {
  if (initialized_) {
    return true;
  }

  if (esp_netif_init() != ESP_OK) {
    return false;
  }
  if (esp_event_loop_create_default() != ESP_OK) {
    return false;
  }

  esp_netif_create_default_wifi_sta();

  wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&init_cfg) != ESP_OK) {
    return false;
  }

  if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
    return false;
  }

  initialized_ = (esp_wifi_start() == ESP_OK);
  return initialized_;
}

bool EspWiFiAdapter::connect(const char* ssid, const char* password) {
  if (!startIfNeeded()) {
    ESP_LOGE(kTag, "Wi-Fi start failed");
    return false;
  }

  wifi_config_t cfg{};
  std::strncpy(reinterpret_cast<char*>(cfg.sta.ssid), ssid, sizeof(cfg.sta.ssid) - 1U);
  std::strncpy(reinterpret_cast<char*>(cfg.sta.password), password, sizeof(cfg.sta.password) - 1U);

  if (esp_wifi_set_config(WIFI_IF_STA, &cfg) != ESP_OK) {
    return false;
  }

  return esp_wifi_connect() == ESP_OK;
}

bool EspWiFiAdapter::isConnected() const {
  wifi_ap_record_t info{};
  return esp_wifi_sta_get_ap_info(&info) == ESP_OK;
}

EspMqttAdapter::~EspMqttAdapter() { destroyClient(); }

void EspMqttAdapter::destroyClient() {
  if (client_ == nullptr) {
    return;
  }

  auto* typed = static_cast<esp_mqtt_client_handle_t>(client_);
  esp_mqtt_client_stop(typed);
  esp_mqtt_client_destroy(typed);
  client_ = nullptr;
  connected_ = false;
}

bool EspMqttAdapter::connect(const model::MQTTConfig& cfg, const std::string& client_id) {
  if (client_ != nullptr) {
    return true;
  }

  esp_mqtt_client_config_t config{};
  config.broker.address.hostname = cfg.broker.c_str();
  config.broker.address.port = cfg.port;
  config.credentials.username = cfg.username.empty() ? nullptr : cfg.username.c_str();
  config.credentials.authentication.password = cfg.password.empty() ? nullptr : cfg.password.c_str();
  config.credentials.client_id = client_id.c_str();

  auto* client = esp_mqtt_client_init(&config);
  if (client == nullptr) {
    return false;
  }

  if (esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, &EspMqttAdapter::onEvent, this) != ESP_OK) {
    esp_mqtt_client_destroy(client);
    return false;
  }

  if (esp_mqtt_client_start(client) != ESP_OK) {
    esp_mqtt_client_destroy(client);
    return false;
  }

  client_ = client;
  return true;
}

void EspMqttAdapter::loop() {}

bool EspMqttAdapter::isConnected() const { return connected_; }

bool EspMqttAdapter::publish(const std::string& topic, const std::string& payload, bool retain) {
  if (client_ == nullptr) {
    return false;
  }

  auto* typed = static_cast<esp_mqtt_client_handle_t>(client_);
  const int id = esp_mqtt_client_publish(typed, topic.c_str(), payload.c_str(), static_cast<int>(payload.size()),
                                         1, retain ? 1 : 0);
  return id >= 0;
}

void EspMqttAdapter::onEvent(void* handler_args, esp_event_base_t, int32_t event_id, void*) {
  auto* self = static_cast<EspMqttAdapter*>(handler_args);
  if (self == nullptr) {
    return;
  }

  if (event_id == MQTT_EVENT_CONNECTED) {
    self->connected_ = true;
    ESP_LOGI(kTag, "MQTT connected");
    return;
  }

  if (event_id == MQTT_EVENT_DISCONNECTED) {
    self->connected_ = false;
    ESP_LOGW(kTag, "MQTT disconnected");
  }
}

esp_err_t EspRestApiAdapter::healthHandler(httpd_req_t* req) {
  static constexpr char kPayload[] = "{\"status\":\"ok\"}";
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, kPayload, HTTPD_RESP_USE_STRLEN);
}

void EspRestApiAdapter::start() {
  if (server_ != nullptr) {
    return;
  }

  httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&server_, &cfg) != ESP_OK) {
    ESP_LOGE(kTag, "HTTP server start failed");
    return;
  }

  httpd_uri_t health{};
  health.uri = "/api/v1/health";
  health.method = HTTP_GET;
  health.handler = &EspRestApiAdapter::healthHandler;
  httpd_register_uri_handler(server_, &health);
}

void EspRestApiAdapter::poll() {}

void EspOtaAdapter::start() {
  initialized_ = true;
  ESP_LOGI(kTag, "OTA adapter started");
}

void EspOtaAdapter::poll() {
  if (!initialized_) {
    return;
  }
}

}  // namespace esp_ir::net

#endif  // ESP_PLATFORM
