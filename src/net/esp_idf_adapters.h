#pragma once

#include <functional>
#include <string>

#include "net/network_services.h"

#ifdef ESP_PLATFORM

#include "esp_http_server.h"

namespace esp_ir::net {

class EspMonotonicClock final : public MonotonicClock {
 public:
  uint64_t nowMs() const override;
};

class EspWiFiAdapter final : public WiFiAdapter {
 public:
  bool connect(const char* ssid, const char* password) override;
  bool isConnected() const override;

 private:
  bool initialized_{false};
  bool startIfNeeded();
};

class EspMqttAdapter final : public MQTTAdapter {
 public:
  EspMqttAdapter() = default;
  ~EspMqttAdapter() override;

  bool connect(const model::MQTTConfig& cfg, const std::string& client_id) override;
  void loop() override;
  bool isConnected() const override;
  bool publish(const std::string& topic, const std::string& payload, bool retain) override;

 private:
  void destroyClient();
  static void onEvent(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);

  void* client_{nullptr};
  bool connected_{false};
};

class EspRestApiAdapter final : public RestApiAdapter {
 public:
  using TriggerCallback = std::function<bool(const std::string&)>;

  explicit EspRestApiAdapter(std::string config_file_path = "/config/system_config.json",
                             std::string api_token = "change-me")
      : config_file_path_(std::move(config_file_path)), api_token_(std::move(api_token)) {}

  void setTriggerCallback(TriggerCallback callback) { trigger_callback_ = std::move(callback); }

  void start() override;
  void poll() override;

 private:
  static esp_err_t healthHandler(httpd_req_t* req);
  static esp_err_t configGetHandler(httpd_req_t* req);
  static esp_err_t configPutHandler(httpd_req_t* req);
  static esp_err_t homesGetHandler(httpd_req_t* req);
  static esp_err_t learnStartHandler(httpd_req_t* req);
  static esp_err_t learnStopHandler(httpd_req_t* req);
  static esp_err_t triggerHandler(httpd_req_t* req);
  static esp_err_t otaStartHandler(httpd_req_t* req);

  static EspRestApiAdapter* fromReq(httpd_req_t* req);
  static bool readRequestBody(httpd_req_t* req, std::string& out);

  bool isAuthorized(httpd_req_t* req) const;

  std::string config_file_path_;
  std::string api_token_;
  TriggerCallback trigger_callback_;
  httpd_handle_t server_{nullptr};
};

class EspOtaAdapter final : public OTAAdapter {
 public:
  void start() override;
  void poll() override;

 private:
  bool initialized_{false};
};

}  // namespace esp_ir::net

#endif  // ESP_PLATFORM
