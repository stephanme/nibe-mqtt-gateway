#include "Relay.h"

#include <esp_log.h>

static const char* TAG = "relay";

MqttRelay::MqttRelay(enum Relay relay, const std::string& name, Metrics& metrics): metrics(metrics) {
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

    JsonDocument discoveryDoc =
        homeassistantDiscoveryMessage(config, mqttClient->getConfig().rootTopic, mqttClient->getDeviceDiscoveryInfoRef());

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

// TODO: quite some code duplication with nibegw_config.cpp, move e.g. to mqtt.cpp
JsonDocument MqttRelay::homeassistantDiscoveryMessage(const MqttRelayConfig& config, const std::string& nibeRootTopic,
                                                      const std::string& deviceDiscoveryInfo) const {
    auto discoveryDoc = defaultHomeassistantDiscoveryMessage(nibeRootTopic, deviceDiscoveryInfo);
    if (!config.homeassistantDiscoveryOverride.empty()) {
        // parse override json and defDiscoveryMsg and merge them
        JsonDocument overrideDoc;
        DeserializationError errOverride = deserializeJson(overrideDoc, config.homeassistantDiscoveryOverride);
        if (errOverride) {
            ESP_LOGE(TAG, "Failed to parse override discovery message for relay %s: %s", name.c_str(), errOverride.c_str());
            return discoveryDoc;
        }
        // merge overrideDoc into discoveryMsgDoc
        for (auto kv : overrideDoc.as<JsonObject>()) {
            if (kv.value().isNull()) {
                discoveryDoc.remove(kv.key());
            } else {
                discoveryDoc[kv.key()] = kv.value();
            }
        }
    }
    return discoveryDoc;
}

JsonDocument MqttRelay::defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                             const std::string& deviceDiscoveryInfo) const {
    JsonDocument discoveryDoc;

    // TODO: ugly, maybe treat discovery info as json everywhere
    char str[deviceDiscoveryInfo.size() + 3];
    str[0] = '{';
    strcpy(str + 1, deviceDiscoveryInfo.c_str());
    str[deviceDiscoveryInfo.size() + 1] = '}';
    str[deviceDiscoveryInfo.size() + 2] = '\0';
    DeserializationError err = deserializeJson(discoveryDoc, str);
    if (err) {
        // should not happen
        ESP_LOGE(TAG, "Failed to parse device discovery info for relay %s: %s", name.c_str(), err.c_str());
    }

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
