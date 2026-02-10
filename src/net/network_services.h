#pragma once

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

}  // namespace esp_ir::net
