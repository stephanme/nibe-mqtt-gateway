#include "Relay.h"

#include <esp_log.h>

static const char* TAG = "relay";

static const char* DISCOVERY_PAYLOAD = R"({
"obj_id":"nibegw-%s",
"uniq_id":"nibegw-%s",
"name":"%s",
"cmd_t":"%s",
"stat_t":"%s",
"pl_on":"ON",
"pl_off":"OFF",
%s
})";

MqttRelay::MqttRelay(const std::string& mqttTopic, const std::string& name, enum Relay relay) {
    this->mqttTopic = mqttTopic;
    this->name = name;
    this->relay = relay;
}

// does not publish state
int MqttRelay::begin(MqttClient* mqttClient) {
    this->mqttClient = mqttClient;
    stateTopic = mqttClient->getConfig().rootTopic + "/" + mqttTopic + "/state";

    ESP_LOGI(TAG, "begin relay %s, channel %d", name.c_str(), relay);
    ESP_LOGI(TAG, "MQTT topic: %s", stateTopic.c_str());

    // subscribe to 'set' topic
    std::string commandTopic = mqttClient->getConfig().rootTopic + "/" + mqttTopic + "/set";
    mqttClient->subscribe(commandTopic, this);

    // publish MQTT discovery, TODO: move to onConnected() callback?
    std::string discoveryTopic = mqttClient->getConfig().discoveryPrefix + "/switch/nibegw/" + mqttTopic + "/config";
    int len = strlen(DISCOVERY_PAYLOAD) + 2 * mqttTopic.length() + name.length() + commandTopic.length() + stateTopic.length() +
              mqttClient->getDeviceDiscoveryInfoRef().length() + 1;
    char discoveryPayload[len];
    snprintf(discoveryPayload, len, DISCOVERY_PAYLOAD, mqttTopic.c_str(), mqttTopic.c_str(), name.c_str(), commandTopic.c_str(),
             stateTopic.c_str(), mqttClient->getDeviceDiscoveryInfoRef().c_str());
    mqttClient->publish(discoveryTopic, discoveryPayload, QOS0, true);
    return 0;
}

void MqttRelay::setRelayState(bool state) {
    ESP_LOGI(TAG, "Relay %s set to %s", name.c_str(), state ? "ON" : "OFF");
    KMPProDinoESP32.setRelayState(relay, state);
    publishState(state);
}

bool MqttRelay::getRelayState(void) { return KMPProDinoESP32.getRelayState(relay); }

void MqttRelay::publishState() { publishState(getRelayState()); }

void MqttRelay::publishState(bool state) { mqttClient->publish(stateTopic, state ? "ON" : "OFF"); }

void MqttRelay::onMqttMessage(const std::string& topic, const std::string& payload) { setRelayState(payload == "ON"); }
