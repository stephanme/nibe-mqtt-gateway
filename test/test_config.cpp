#include <unity.h>
#include <regex>

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

static const char* nibeModbusConfig = R"(ModbusManager 1.0.9
20200624
Product: VVM310, VVM500
Database: 8310
Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
"BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
)";

TEST_CASE("parseNibeModbusCSV", "[config]") {
    std::unordered_map<uint16_t, Coil> coils;
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV("", coils));
    std::string badConfig = std::string(nibeModbusConfig);
    badConfig = std::regex_replace(badConfig, std::regex(";Factor;"), ";XFactor;");
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV(badConfig.c_str(), coils));

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(nibeModbusConfig, coils));
    TEST_ASSERT_EQUAL(1, coils.size());
    TEST_ASSERT(coils[40004] == Coil(40004, "BT1 Outdoor Temperature", "Current outdoor temperature", "°C", COIL_DATA_TYPE_INT16, 10, 0, 0, 0, COIL_MODE_READ));
}

TEST_CASE("parseNibeModbusCSVLine", "[config]") {
    Coil coil;
    
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine("", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine("bad csv", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"(title";"info";40004;"unit";s16;10;0;0;0;R;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info;40004;"unit";s16;10;0;0;0;R;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;unit;s16;10;0;0;0;R;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"unit";x16;10;0;0;0;R;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"unit";s16;10;0;0;0;X;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";xxx;"unit";s16;10;0;0;0;R;)", coil));
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"unit";s16;;0;0;0;R;)", coil));

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"unit";s16;10;0;0;0;R;)", coil));
    TEST_ASSERT(coil == Coil(40004, "title", "info", "unit", COIL_DATA_TYPE_INT16, 10, 0, 0, 0, COIL_MODE_READ));
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"unit";u32;1;100;200;1;RW;)", coil));
    TEST_ASSERT(coil == Coil(40004, "title", "info", "unit", COIL_DATA_TYPE_UINT32, 1, 100, 200, 1, COIL_MODE_READ_WRITE));
}
