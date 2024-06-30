#include <unity.h>

#include "Relay.h"
#include "mqtt_mock.h"
//
#include <ArduinoJson.h>

TEST_CASE("configuration", "[relay]") {
    MqttConfig mqttConfig = {.brokerUri = "mqtt://localhost",
                             .clientId = "clientid",
                             .rootTopic = "nibegw",
                             .discoveryPrefix = "homeassistant",
                             .deviceName = "Nibe GW",
                             .deviceManufacturer = "Nibe",
                             .deviceModel = "Heatpump",
                             .deviceConfigurationUrl = "http://nibegw"};
    Metrics metrics;
    MqttClient mqttClient = MqttClient(metrics);
    mqttClient.begin(mqttConfig);

    MqttRelay relay = MqttRelay(Relay1, "r", metrics);
    MqttRelayConfig config = {.name = "myrelay-1", .homeassistantDiscoveryOverride = R"({"name":"Relay 1"})"};
    mqttmock_publishData.clear();
    relay.begin(config, &mqttClient);

    TEST_ASSERT_EQUAL_STRING("myrelay-1", relay.getName().c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw/relay/myrelay-1/state", relay.getStateTopic().c_str());
    TEST_ASSERT_EQUAL(1, mqttmock_publishData.size());

    TEST_ASSERT_EQUAL_STRING("homeassistant/switch/nibegw/myrelay-1/config", mqttmock_publishData[0].topic.c_str());
    JsonDocument discoveryDoc;
    DeserializationError error = deserializeJson(discoveryDoc, mqttmock_publishData[0].payload);
    TEST_ASSERT(error == DeserializationError::Ok);
    TEST_ASSERT_EQUAL_STRING("nibegw-myrelay-1", discoveryDoc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-myrelay-1", discoveryDoc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Relay 1", discoveryDoc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/relay/myrelay-1/state", discoveryDoc["stat_t"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/relay/myrelay-1/set", discoveryDoc["cmd_t"]);
    TEST_ASSERT_EQUAL_STRING("ON", discoveryDoc["pl_on"]);
    TEST_ASSERT_EQUAL_STRING("OFF", discoveryDoc["pl_off"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/availability", discoveryDoc["avty_t"]);

    Metric* m = metrics.findMetric(R"(nibegw_relay_state{relay="1",name="myrelay-1"})");
    TEST_ASSERT_NOT_NULL(m);
}
