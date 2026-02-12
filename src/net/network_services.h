#pragma once

#include <cstdint>
#include <string>

#include "model/ir_model.h"

namespace esp_ir::net {

class WiFiService {
 public:
  virtual ~WiFiService() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
  virtual bool connected() const = 0;
};

class MQTTService {
 public:
  virtual ~MQTTService() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
  virtual bool connected() const = 0;
  virtual void publishDiscovery() = 0;
};

class RestApiService {
 public:
  virtual ~RestApiService() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
};

class OTAService {
 public:
  virtual ~OTAService() = default;
  virtual void begin() = 0;
  virtual void loop() = 0;
};

class MonotonicClock {
 public:
  virtual ~MonotonicClock() = default;
  virtual uint64_t nowMs() const = 0;
};

class WiFiAdapter {
 public:
  virtual ~WiFiAdapter() = default;
  virtual bool connect(const char* ssid, const char* password) = 0;
  virtual bool isConnected() const = 0;
};

class MQTTAdapter {
 public:
  virtual ~MQTTAdapter() = default;
  virtual bool connect(const model::MQTTConfig& cfg, const std::string& client_id) = 0;
  virtual void loop() = 0;
  virtual bool isConnected() const = 0;
  virtual bool publish(const std::string& topic, const std::string& payload, bool retain) = 0;
};

class RestApiAdapter {
 public:
  virtual ~RestApiAdapter() = default;
  virtual void start() = 0;
  virtual void poll() = 0;
};

class OTAAdapter {
 public:
  virtual ~OTAAdapter() = default;
  virtual void start() = 0;
  virtual void poll() = 0;
};

class WiFiServiceImpl final : public WiFiService {
 public:
  WiFiServiceImpl(const model::WiFiConfig& config, WiFiAdapter& adapter, MonotonicClock& clock)
      : config_(config), adapter_(adapter), clock_(clock) {}

  void begin() override;
  void loop() override;
  bool connected() const override;

 private:
  model::WiFiConfig config_;
  WiFiAdapter& adapter_;
  MonotonicClock& clock_;
  uint64_t last_attempt_ms_{0};
};

class MQTTServiceImpl final : public MQTTService {
 public:
  MQTTServiceImpl(const model::MQTTConfig& config, std::string client_id, MQTTAdapter& adapter,
                  WiFiService& wifi)
      : config_(config), client_id_(std::move(client_id)), adapter_(adapter), wifi_(wifi) {}

  void begin() override;
  void loop() override;
  bool connected() const override;
  void publishDiscovery() override;

 private:
  model::MQTTConfig config_;
  std::string client_id_;
  MQTTAdapter& adapter_;
  WiFiService& wifi_;
  bool discovery_published_{false};
};

class RestApiServiceImpl final : public RestApiService {
 public:
  explicit RestApiServiceImpl(RestApiAdapter& adapter) : adapter_(adapter) {}

  void begin() override;
  void loop() override;

 private:
  RestApiAdapter& adapter_;
};

class OTAServiceImpl final : public OTAService {
 public:
  OTAServiceImpl(OTAAdapter& adapter, bool enabled) : adapter_(adapter), enabled_(enabled) {}

  void begin() override;
  void loop() override;

 private:
  OTAAdapter& adapter_;
  bool enabled_{true};
};

}  // namespace esp_ir::net
