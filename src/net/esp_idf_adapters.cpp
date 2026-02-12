#include "net/esp_idf_adapters.h"

#ifdef ESP_PLATFORM

#include <cstdio>
#include <cstring>

#include "cJSON.h"
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
constexpr size_t kMaxBodyBytes = 8192;

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

cJSON* parseJsonBody(httpd_req_t* req, std::string& raw_body) {
  if (req == nullptr) {
    return nullptr;
  }

  if (req->content_len <= 0 || static_cast<size_t>(req->content_len) > kMaxBodyBytes) {
    return nullptr;
  }

  raw_body.resize(static_cast<size_t>(req->content_len));
  int offset = 0;
  while (offset < req->content_len) {
    const int read_len = httpd_req_recv(req, raw_body.data() + offset, req->content_len - offset);
    if (read_len <= 0) {
      return nullptr;
    }
    offset += read_len;
  }

  return cJSON_Parse(raw_body.c_str());
}

bool readFileToString(const std::string& path, std::string& out) {
  std::FILE* fp = std::fopen(path.c_str(), "rb");
  if (fp == nullptr) {
    return false;
  }

  std::fseek(fp, 0, SEEK_END);
  const long size = std::ftell(fp);
  std::fseek(fp, 0, SEEK_SET);
  if (size <= 0) {
    std::fclose(fp);
    return false;
  }

  out.resize(static_cast<size_t>(size));
  const size_t read_size = std::fread(out.data(), 1, out.size(), fp);
  std::fclose(fp);
  return read_size == out.size();
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
  if (req == nullptr || req->content_len <= 0 || static_cast<size_t>(req->content_len) > kMaxBodyBytes) {
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

bool EspRestApiAdapter::isAuthorized(httpd_req_t* req) const {
  if (api_token_.empty()) {
    return true;
  }

  char header_value[128] = {0};
  if (httpd_req_get_hdr_value_str(req, "X-API-Key", header_value, sizeof(header_value)) != ESP_OK) {
    return false;
  }

  return api_token_ == header_value;
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

  std::string config_text;
  if (!readFileToString(self->config_file_path_, config_text)) {
    sendJson(req, "{\"error\":\"config_not_found_or_invalid\"}", "500 Internal Server Error");
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

  if (!self->isAuthorized(req)) {
    sendJson(req, "{\"error\":\"unauthorized\"}", "401 Unauthorized");
    return ESP_FAIL;
  }

  std::string payload;
  cJSON* root = parseJsonBody(req, payload);
  if (root == nullptr) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }
  cJSON_Delete(root);

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

  std::string config_text;
  if (!readFileToString(self->config_file_path_, config_text)) {
    sendJson(req, "{\"error\":\"config_not_found\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  storage::ConfigManager config_manager;
  model::SystemConfig parsed;
  if (!config_manager.parse(config_text, parsed)) {
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
  auto* self = fromReq(req);
  if (self == nullptr || !self->isAuthorized(req)) {
    sendJson(req, "{\"error\":\"unauthorized\"}", "401 Unauthorized");
    return ESP_FAIL;
  }

  runtimeState().learning_active = true;
  sendJson(req, "{\"status\":\"learn_started\"}");
  return ESP_OK;
}

esp_err_t EspRestApiAdapter::learnStopHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr || !self->isAuthorized(req)) {
    sendJson(req, "{\"error\":\"unauthorized\"}", "401 Unauthorized");
    return ESP_FAIL;
  }

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

  if (!self->isAuthorized(req)) {
    sendJson(req, "{\"error\":\"unauthorized\"}", "401 Unauthorized");
    return ESP_FAIL;
  }

  std::string body;
  cJSON* root = parseJsonBody(req, body);
  if (root == nullptr) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const cJSON* button_id_node = cJSON_GetObjectItemCaseSensitive(root, "button_id");
  if (!cJSON_IsString(button_id_node) || button_id_node->valuestring == nullptr ||
      std::strlen(button_id_node->valuestring) == 0) {
    cJSON_Delete(root);
    sendJson(req, "{\"error\":\"button_id_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const std::string button_id = button_id_node->valuestring;
  cJSON_Delete(root);

  const bool sent = self->trigger_callback_(button_id);
  sendJson(req, sent ? "{\"status\":\"sent\"}" : "{\"status\":\"not_found\"}",
           sent ? "200 OK" : "400 Bad Request");
  return sent ? ESP_OK : ESP_FAIL;
}

esp_err_t EspRestApiAdapter::otaStartHandler(httpd_req_t* req) {
  auto* self = fromReq(req);
  if (self == nullptr) {
    sendJson(req, "{\"error\":\"adapter_context_missing\"}", "500 Internal Server Error");
    return ESP_FAIL;
  }

  if (!self->isAuthorized(req)) {
    sendJson(req, "{\"error\":\"unauthorized\"}", "401 Unauthorized");
    return ESP_FAIL;
  }

  std::string body;
  cJSON* root = parseJsonBody(req, body);
  if (root == nullptr) {
    sendJson(req, "{\"error\":\"request_body_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  const cJSON* url_node = cJSON_GetObjectItemCaseSensitive(root, "url");
  if (!cJSON_IsString(url_node) || url_node->valuestring == nullptr || std::strlen(url_node->valuestring) < 8 ||
      std::strncmp(url_node->valuestring, "https://", 8) != 0) {
    cJSON_Delete(root);
    sendJson(req, "{\"error\":\"url_invalid\"}", "400 Bad Request");
    return ESP_FAIL;
  }

  runtimeState().pending_ota_url = url_node->valuestring;
  cJSON_Delete(root);

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
