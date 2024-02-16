#include "Relay.h"

#include <esp_log.h>

static const char* TAG = "relay";

static const char* DISCOVERY_PAYLOAD = R"({
"object_id":"nibegw-%s",
"unique_id":"nibegw-%s",
"name":"%s",
"command_topic":"%s",
"state_topic":"%s",
"payload_on":"ON",
"payload_off":"OFF",
%s
})";

MqttRelay::MqttRelay(const String& mqttTopic, const String& name, enum Relay relay) {
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
    String commandTopic = mqttClient->getConfig().rootTopic + "/" + mqttTopic + "/set";
    mqttClient->subscribe(commandTopic, this);

    // publish MQTT discovery, TODO: move to onConnected() callback?
    String discoveryTopic = mqttClient->getConfig().discoveryPrefix + "/switch/" + mqttTopic + "/config";
    char discoveryPayload[strlen(DISCOVERY_PAYLOAD) + 2 * mqttTopic.length() + name.length() + commandTopic.length() +
                          stateTopic.length() + mqttClient->getDeviceDiscoveryInfo().length() + 1];
    sprintf(discoveryPayload, DISCOVERY_PAYLOAD, mqttTopic.c_str(), mqttTopic.c_str(), name.c_str(), commandTopic.c_str(),
            stateTopic.c_str(), mqttClient->getDeviceDiscoveryInfo().c_str());
    mqttClient->publish(discoveryTopic, discoveryPayload, 0, 1);
    return 0;
}

void MqttRelay::setRelayState(bool state) {
    ESP_LOGI(TAG, "Relay %s set to %s", name.c_str(), state ? "ON" : "OFF");
    KMPProDinoESP32.setRelayState(relay, state);
    publishState(state);
}

bool MqttRelay::getRelayState(void) { return KMPProDinoESP32.getRelayState(relay); }

void MqttRelay::publishState() { publishState(getRelayState()); }

void MqttRelay::publishState(bool state) {
    String payload = state ? "ON" : "OFF";
    mqttClient->publish(stateTopic, payload);
}

void MqttRelay::onMqttMessage(const String& topic, const String& payload) { setRelayState(payload == "ON"); }