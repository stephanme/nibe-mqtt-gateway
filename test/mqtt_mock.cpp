#include "mqtt_mock.h"

#include "config.h"

MqttClient::MqttClient(Metrics& metrics) : metricMqttStatus(metrics.addMetric(METRIC_NAME_MQTT_STATUS, 1)) {
    metricMqttStatus.setValue((int32_t)MqttStatus::Disconnected);
}

esp_err_t MqttClient::begin(const MqttConfig& config) {
    this->config = &config;

    availabilityTopic.reserve(config.rootTopic.length() + 13);  // "/availability"
    availabilityTopic = config.rootTopic;
    availabilityTopic += "/availability";

    // https://www.home-assistant.io/integrations/mqtt/#discovery-payload
    deviceDiscoveryInfo["avty_t"] = availabilityTopic;
    deviceDiscoveryInfo["dev"]["name"] = config.deviceName;
    deviceDiscoveryInfo["dev"]["ids"].add(config.clientId);
    deviceDiscoveryInfo["dev"]["mf"] = config.deviceManufacturer;
    deviceDiscoveryInfo["dev"]["mdl"] = config.deviceModel;
    deviceDiscoveryInfo["dev"]["sw"] = "1.0";
    deviceDiscoveryInfo["dev"]["cu"] = config.deviceConfigurationUrl;

    deviceDiscoveryInfoRef["avty_t"] = availabilityTopic;
    deviceDiscoveryInfoRef["dev"]["ids"].add(config.clientId);

    return ESP_OK;
}

esp_err_t MqttClient::registerLifecycleCallback(MqttClientLifecycleCallback* callback) { return ESP_OK; }
int MqttClient::publishAvailability() { return 0; }

std::vector<MqttPublishData> mqttmock_publishData;
int MqttClient::publish(const std::string& topic, const std::string& payload, MqttQOS qos, bool retain) {
    return publish(topic, payload.c_str(), 0, qos, retain);
}
int MqttClient::publish(const std::string& topic, const char* payload, MqttQOS qos, bool retain) {
    return publish(topic, payload, 0, qos, retain);
}
int MqttClient::publish(const std::string& topic, const char* payload, int length, MqttQOS qos, bool retain) {
    MqttPublishData data = {topic, std::string(payload), qos, retain};
    mqttmock_publishData.push_back(data);
    return 0;
}

int MqttClient::subscribe(const std::string& topic, MqttSubscriptionCallback* callback, int qos) { return 0; }
