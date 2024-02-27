#include <unity.h>

#include "config.h"

TEST_CASE("default config", "[config]") {
    NibeMqttGwConfigManager configManager;
    const NibeMqttGwConfig& config = configManager.getConfig();
    TEST_ASSERT_EQUAL_STRING("mqtt://mosquitto", config.mqtt.brokerUri.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.mqtt.clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.rootTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.mqtt.discoveryPrefix.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.mqtt.hostname.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw/log", config.mqtt.logTopic.c_str());

    TEST_ASSERT_FALSE(config.logging.mqttLoggingEnabled);
    TEST_ASSERT_TRUE(config.logging.stdoutLoggingEnabled);
    TEST_ASSERT_EQUAL_STRING("nibegw/log", config.logging.logTopic.c_str());

    configManager.begin();
    TEST_ASSERT_EQUAL_STRING("nibegw-00:00:00:00:00:00", config.mqtt.clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.rootTopic.c_str());
}

static const char* configJson = R"({
    "mqtt": {
        "brokerUri": "mqtt://mosquitto.fritz.box",
        "user": "user",
        "password": "password",
        "rootTopic": "nibegw",
        "discoveryPrefix": "homeassistant"
    },
    "nibe": {
        "coilsToPoll": [1,2]
    },
    "logging": {
        "mqttLoggingEnabled": true,
        "stdoutLoggingEnabled": true,
        "logTopic": "nibegw/logs"
    }
})";

TEST_CASE("saveConfig", "[config]") {
    NibeMqttGwConfigManager configManager;
    configManager.begin();
    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(configJson));

    const NibeMqttGwConfig& config = configManager.getConfig();
    TEST_ASSERT_EQUAL_STRING("mqtt://mosquitto.fritz.box", config.mqtt.brokerUri.c_str());
    TEST_ASSERT_EQUAL_STRING("user", config.mqtt.user.c_str());
    TEST_ASSERT_EQUAL_STRING("password", config.mqtt.password.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw-00:00:00:00:00:00", config.mqtt.clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.rootTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.mqtt.discoveryPrefix.c_str());

    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.hostname.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw/logs", config.mqtt.logTopic.c_str());

    TEST_ASSERT_EQUAL(2, config.nibe.coilsToPoll.size());
    TEST_ASSERT_EQUAL(1, config.nibe.coilsToPoll[0]);
    TEST_ASSERT_EQUAL(2, config.nibe.coilsToPoll[1]);

    TEST_ASSERT_TRUE(config.logging.mqttLoggingEnabled);
    TEST_ASSERT_TRUE(config.logging.stdoutLoggingEnabled);
    TEST_ASSERT_EQUAL_STRING("nibegw/logs", config.logging.logTopic.c_str());
}

TEST_CASE("getConfigAsJson", "[config]") {
    NibeMqttGwConfigManager configManager;
    configManager.begin();

    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(configJson));
    std::string json = configManager.getConfigAsJson();
    TEST_ASSERT_TRUE(json.contains("mqtt://mosquitto.fritz.box"));
    TEST_ASSERT_TRUE(json.contains("nibegw/logs"));

    // test that returned json can be saved again
    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(json.c_str()));
}