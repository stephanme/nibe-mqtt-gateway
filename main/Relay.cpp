#include "Relay.h"

#include <esp_log.h>

#include "mqtt_helper.h"

static const char* TAG = "relay";

MqttRelay::MqttRelay(enum Relay relay, const std::string& name, Metrics& metrics) : metrics(metrics) {
    this->relay = relay;
    this->name = name;
}

// does not publish state
int MqttRelay::begin(const MqttRelayConfig& config, MqttClient* mqttClient) {
    this->mqttClient = mqttClient;
    name = config.name;

    char metricName[64];
    snprintf(metricName, sizeof(metricName), R"(nibegw_relay_state{relay="%d",name="%s"})", relay + 1, name.c_str());
    metricRelayState = &metrics.addMetric(metricName);
    metricRelayState->setValue(getRelayState() ? 1 : 0);

    ESP_LOGI(TAG, "begin relay %s, channel %d", name.c_str(), relay);

    JsonDocument discoveryDoc = homeassistantDiscoveryMessage(config, *mqttClient);

    stateTopic = discoveryDoc["stat_t"].as<std::string>();
    ESP_LOGI(TAG, "MQTT topic: %s", stateTopic.c_str());
    // subscribe to command topic
    mqttClient->subscribe(discoveryDoc["cmd_t"].as<std::string>(), this);

    // publish MQTT discovery, TODO: move to onConnected() callback?
    char discoveryTopic[64];
    const char* component = discoveryDoc["_component_"] | "switch";
    snprintf(discoveryTopic, sizeof(discoveryTopic), "%s/%s/nibegw/%s/config", mqttClient->getConfig().discoveryPrefix.c_str(),
             component, name.c_str());

    discoveryDoc.remove("_component_");
    std::string discoveryMsg;
    serializeJson(discoveryDoc, discoveryMsg);
    mqttClient->publish(discoveryTopic, discoveryMsg, QOS0, true);

    return 0;
}

JsonDocument MqttRelay::homeassistantDiscoveryMessage(const MqttRelayConfig& config, const MqttClient& mqttClient) const {
    JsonDocument discoveryDoc = mqttClient.getDeviceDiscoveryInfoRef();
    const std::string& nibeRootTopic = mqttClient.getConfig().rootTopic;

    discoveryDoc["_component_"] = "switch";
    char objId[64];
    snprintf(objId, sizeof(objId), "nibegw-%s", name.c_str());
    discoveryDoc["obj_id"] = objId;
    discoveryDoc["uniq_id"] = objId;
    discoveryDoc["name"] = name.c_str();
    discoveryDoc["stat_t"] = nibeRootTopic + "/relay/" + name + "/state";
    discoveryDoc["cmd_t"] = nibeRootTopic + "/relay/" + name + "/set";
    discoveryDoc["pl_on"] = "ON";
    discoveryDoc["pl_off"] = "OFF";

    MqttHelper::mergeMqttDiscoveryInfoOverride(discoveryDoc, config.homeassistantDiscoveryOverride);
    return discoveryDoc;
}

void MqttRelay::setRelayState(bool state) {
    ESP_LOGI(TAG, "Relay %s set to %s", name.c_str(), state ? "ON" : "OFF");
    KMPProDinoESP32.setRelayState(relay, state);
    publishState(state);
}

bool MqttRelay::getRelayState(void) { return KMPProDinoESP32.getRelayState(relay); }

void MqttRelay::publishState() { publishState(getRelayState()); }

void MqttRelay::publishState(bool state) {
    if (state) {
        metricRelayState->setValue(1);
        mqttClient->publish(stateTopic, "ON");
    } else {
        metricRelayState->setValue(0);
        mqttClient->publish(stateTopic, "OFF");
    }
}

void MqttRelay::onMqttMessage(const std::string& topic, const std::string& payload) { setRelayState(payload == "ON"); }
