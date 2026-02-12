#include "net/network_services.h"

namespace esp_ir::net {

void WiFiServiceImpl::begin() {
  last_attempt_ms_ = 0;
  loop();
}

void WiFiServiceImpl::loop() {
  if (adapter_.isConnected()) {
    return;
  }

  const auto now_ms = clock_.nowMs();
  const uint64_t reconnect_interval_ms = static_cast<uint64_t>(config_.reconnect_interval_sec) * 1000ULL;

  if (last_attempt_ms_ != 0 && (now_ms - last_attempt_ms_) < reconnect_interval_ms) {
    return;
  }

  (void)adapter_.connect(config_.ssid.c_str(), config_.password.c_str());
  last_attempt_ms_ = now_ms;
}

bool WiFiServiceImpl::connected() const { return adapter_.isConnected(); }

void MQTTServiceImpl::begin() {
  if (!config_.enabled) {
    return;
  }

  if (!wifi_.connected()) {
    return;
  }

  (void)adapter_.connect(config_, client_id_);
}

void MQTTServiceImpl::loop() {
  if (!config_.enabled || !wifi_.connected()) {
    return;
  }

  if (!adapter_.isConnected()) {
    (void)adapter_.connect(config_, client_id_);
    return;
  }

  adapter_.loop();

  if (config_.ha_discovery && !discovery_published_) {
    publishDiscovery();
  }
}

bool MQTTServiceImpl::connected() const { return config_.enabled && adapter_.isConnected(); }

void MQTTServiceImpl::publishDiscovery() {
  if (!config_.enabled || !adapter_.isConnected()) {
    return;
  }

  const std::string topic = "homeassistant/device/" + client_id_ + "/config";
  const std::string payload = "{\"name\":\"" + client_id_ + "\",\"state_topic\":\"" +
                              config_.base_topic + "/status\"}";

  if (adapter_.publish(topic, payload, config_.retain)) {
    discovery_published_ = true;
  }
}

void RestApiServiceImpl::begin() { adapter_.start(); }

void RestApiServiceImpl::loop() { adapter_.poll(); }

void OTAServiceImpl::begin() {
  if (!enabled_) {
    return;
  }

  adapter_.start();
}

void OTAServiceImpl::loop() {
  if (!enabled_) {
    return;
  }

  adapter_.poll();
}

}  // namespace esp_ir::net
