#include "net/esp_idf_adapters.h"

#ifdef ESP_PLATFORM

#include <cstdio>
#include <cstring>

#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "storage/config_manager.h"

namespace esp_ir::net {
namespace {
constexpr const char* kTag = "esp-idf-net";

struct RuntimeState {
  std::string pending_ota_url;
  bool learning_active{false};
};

RuntimeState& runtimeState() {
  static RuntimeState state;
  return state;
}

void sendJson(httpd_req_t* req, const char* payload, const char* status = "200 OK") {
  httpd_resp_set_status(req, status);
  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
}

}  // namespace

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

EspRestApiAdapter* EspRestApiAdapter::fromReq(httpd_req_t* req) {
  return (req == nullptr || req->user_ctx == nullptr) ? nullptr : static_cast<EspRestApiAdapter*>(req->user_ctx);
}

bool EspRestApiAdapter::readRequestBody(httpd_req_t* req, std::string& out) {
  if (req == nullptr || req->content_len <= 0) {
    return false;
  }

  out.resize(static_cast<size_t>(req->content_len));
  int offset = 0;
  while (offset < req->content_len) {
    const int read_len = httpd_req_recv(req, out.data() + offset, req->content_len - offset);
    if (read_len <= 0) {
      return false;
    }
    offset += read_len;
  }
  return true;
}

esp_err_t EspRestApiAdapter::healthHandler(httpd_req_t* req) {
  sendJson(req, "{\"status\":\"ok\"}");
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::configGetHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr) {
    sendJson(req, "{\"error\":\"adapter_context_missing\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::FILE* fp = std::fopen(self->config_file_path_.c_str(), "rb");
  if (fp == nullptr) {
    sendJson(req, "{\"error\":\"config_not_found\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::fseek(fp, 0, SEEK_END);
  const long size = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (size <= 0) {
    std::fclose(fp);
    sendJson(req, "{\"error\":\"config_empty\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::string config_text(static_cast<size_t>(size), '\0');
  const size_t read = std::fread(config_text.data(), 1, config_text.size(), fp);
  std::fclose(fp);
  if (read != config_text.size()) {
    sendJson(req, "{\"error\":\"config_read_failed\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, config_text.c_str(), static_cast<ssize_t>(config_text.size()));
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::configPutHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr) {
    sendJson(req, "{\"error\":\"adapter_context_missing\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::string payload;
  if (!readRequestBody(req, payload)) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  storage::ConfigManager config_manager;
  auto validation = config_manager.validateJsonText(payload);
  if (!validation.ok) {
    sendJson(req, "{\"error\":\"invalid_config\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  std::FILE* fp = std::fopen(self->config_file_path_.c_str(), "wb");
  if (fp == nullptr) {
    sendJson(req, "{\"error\":\"config_write_failed\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  const size_t written = std::fwrite(payload.data(), 1, payload.size(), fp);
  std::fclose(fp);
  if (written != payload.size()) {
    sendJson(req, "{\"error\":\"config_write_failed\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  sendJson(req, "{\"status\":\"config_saved\"}");
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::homesGetHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr) {
    sendJson(req, "{\"error\":\"adapter_context_missing\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::FILE* fp = std::fopen(self->config_file_path_.c_str(), "rb");
  if (fp == nullptr) {
    sendJson(req, "{\"error\":\"config_not_found\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }
  std::fseek(fp, 0, SEEK_END);
  const long size = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  std::string config_text(static_cast<size_t>(size > 0 ? size : 0), '\0');
  if (!config_text.empty()) {
    (void)std::fread(config_text.data(), 1, config_text.size(), fp);
  }
  std::fclose(fp);

  storage::ConfigManager config_manager;
  model::SystemConfig parsed;
  if (config_text.empty() || !config_manager.parse(config_text, parsed)) {
    sendJson(req, "{\"error\":\"config_parse_failed\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::string response = "{\"homes\":[";
  for (size_t i = 0; i < parsed.homes.size(); ++i) {
    const auto& home = parsed.homes[i];
    response += "{\"home_id\":\"" + home.home_id + "\",\"name\":\"" + home.name + "\"}";
    if (i + 1U < parsed.homes.size()) {
      response += ",";
    }
  }
  response += "]}";

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, response.c_str(), static_cast<ssize_t>(response.size()));
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::learnStartHandler(httpd_req_t* req) {
  runtimeState().learning_active = true;
  sendJson(req, "{\"status\":\"learn_started\"}");
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::learnStopHandler(httpd_req_t* req) {
  runtimeState().learning_active = false;
  sendJson(req, "{\"status\":\"learn_stopped\"}");
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::triggerHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr || !self->trigger_callback_) {
    sendJson(req, "{\"error\":\"trigger_unavailable\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  std::string body;
  if (!readRequestBody(req, body)) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const std::string key = "\"button_id\":\"";
  const auto pos = body.find(key);
  if (pos == std::string::npos) {
    sendJson(req, "{\"error\":\"button_id_missing\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const auto start = pos + key.size();
  const auto end = body.find('"', start);
  if (end == std::string::npos || end <= start) {
    sendJson(req, "{\"error\":\"button_id_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const std::string button_id = body.substr(start, end - start);
  const bool sent = self->trigger_callback_(button_id);
  sendJson(req, sent ? "{\"status\":\"sent\"}" : "{\"status\":\"not_found\"}", sent ? "200 OK" : "400 Bad Request");
  return sent ? ESP_OK : ESP_FAIL;
}

esp_err_t EspRestApiAdapter::otaStartHandler(httpd_req_t* req) {
  std::string body;
  if (!readRequestBody(req, body)) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const std::string key = "\"url\":\"";
  const auto pos = body.find(key);
  if (pos == std::string::npos) {
    sendJson(req, "{\"error\":\"url_missing\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const auto start = pos + key.size();
  const auto end = body.find('"', start);
  if (end == std::string::npos || end <= start) {
    sendJson(req, "{\"error\":\"url_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  runtimeState().pending_ota_url = body.substr(start, end - start);
  sendJson(req, "{\"status\":\"ota_queued\"}");
  return ESP_OK;
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

  auto registerRoute = [this](const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*)) {
    httpd_uri_t route{};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    route.user_ctx = this;
    if (httpd_register_uri_handler(server_, &route) != ESP_OK) {
      ESP_LOGW(kTag, "Failed to register route %s", uri);
    }
  };

  registerRoute("/api/v1/health", HTTP_GET, &EspRestApiAdapter::healthHandler);
  registerRoute("/api/v1/config", HTTP_GET, &EspRestApiAdapter::configGetHandler);
  registerRoute("/api/v1/config", HTTP_PUT, &EspRestApiAdapter::configPutHandler);
  registerRoute("/api/v1/homes", HTTP_GET, &EspRestApiAdapter::homesGetHandler);
  registerRoute("/api/v1/learn/start", HTTP_POST, &EspRestApiAdapter::learnStartHandler);
  registerRoute("/api/v1/learn/stop", HTTP_POST, &EspRestApiAdapter::learnStopHandler);
  registerRoute("/api/v1/trigger", HTTP_POST, &EspRestApiAdapter::triggerHandler);
  registerRoute("/api/v1/ota", HTTP_POST, &EspRestApiAdapter::otaStartHandler);
}

void EspRestApiAdapter::poll() {}

void EspOtaAdapter::start() {
  initialized_ = true;
  ESP_LOGI(kTag, "OTA adapter started");
}

void EspOtaAdapter::poll() {
  if (!initialized_ || runtimeState().pending_ota_url.empty()) {
    return;
  }

  const std::string ota_url = runtimeState().pending_ota_url;
  runtimeState().pending_ota_url.clear();

  ESP_LOGI(kTag, "Starting OTA from %s", ota_url.c_str());
  esp_http_client_config_t http_config{};
  http_config.url = ota_url.c_str();
  http_config.timeout_ms = 10000;

  esp_https_ota_config_t ota_config{};
  ota_config.http_config = &http_config;

  const esp_err_t rc = esp_https_ota(&ota_config);
  if (rc == ESP_OK) {
    ESP_LOGI(kTag, "OTA successful, restarting");
    esp_restart();
    return;
  }

  ESP_LOGE(kTag, "OTA failed rc=%d", static_cast<int>(rc));
}

}  // namespace esp_ir::net

#endif  // ESP_PLATFORM
