#include "mqtt_mock.h"
#include "config.h"

// https://www.home-assistant.io/integrations/mqtt/#discovery-payload
static const char* DEVICE_DISCOVERY_INFO_FULL = R"(
"avty_t":"%s",
"dev":{"name":"%s","ids":["%s"],"mf":"%s","mdl":"%s","sw":"%s","cu":"%s"}
)";
static const char* DEVICE_DISCOVERY_INFO_REF = R"(
"avty_t":"%s",
"dev":{"ids":["%s"]}
)";

MqttClient::MqttClient(Metrics& metrics) : metricMqttStatus(metrics.addMetric(METRIC_NAME_MQTT_STATUS, 1)) {
    metricMqttStatus.setValue((int32_t)MqttStatus::Disconnected);
}

esp_err_t MqttClient::begin(const MqttConfig& config) {
    this->config = &config;

    availabilityTopic.reserve(config.rootTopic.length() + 13);  // "/availability"
    availabilityTopic = config.rootTopic;
    availabilityTopic += "/availability";

    char str[512];
    snprintf(str, sizeof(str), DEVICE_DISCOVERY_INFO_FULL, availabilityTopic.c_str(), config.deviceName.c_str(),
             config.clientId.c_str(), config.deviceManufacturer.c_str(), config.deviceModel.c_str(), "1.0",
             config.deviceConfigurationUrl.c_str());
    deviceDiscoveryInfo = str;
    snprintf(str, sizeof(str), DEVICE_DISCOVERY_INFO_REF, availabilityTopic.c_str(), config.clientId.c_str());
    deviceDiscoveryInfoRef = str;

    return ESP_OK;
}

esp_err_t MqttClient::registerLifecycleCallback(MqttClientLifecycleCallback* callback) { return ESP_OK; }
int MqttClient::publishAvailability() { return 0;}

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

int MqttClient::subscribe(const std::string& topic, MqttSubscriptionCallback* callback, int qos) { return 0;}
