#pragma once

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
  void start() override;
  void poll() override;

 private:
  static esp_err_t healthHandler(httpd_req_t* req);

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
