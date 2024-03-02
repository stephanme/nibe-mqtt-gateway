#include <unistd.h>
#include <unity.h>

#include <fstream>
#include <regex>
#include <sstream>

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

// ugly concatination because of ISO-8859-1 for °C
static const char* nibeModbusConfig = R"(ModbusManager 1.0.9
20200624
Product: VVM310, VVM500
Database: 8310
Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
"BT1 Outdoor Temperature";"Current outdoor temperature";40004;")"
                                      "\xB0"
                                      R"(C";s16;10;0;0;0;R;)";

TEST_CASE("parseNibeModbusCSV", "[config]") {
    std::unordered_map<uint16_t, Coil> coils;
    string_deduplicate_t coilInfoSet;
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV("", &coils, &coilInfoSet));
    std::string badConfig = std::string(nibeModbusConfig);
    badConfig = std::regex_replace(badConfig, std::regex(";Factor;"), ";XFactor;");
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV(badConfig.c_str(), &coils, &coilInfoSet));

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(nibeModbusConfig, &coils, &coilInfoSet));
    TEST_ASSERT_EQUAL(1, coils.size());
    TEST_ASSERT_EQUAL(1, coilInfoSet.size());
    TEST_ASSERT(coils[40004] == Coil(40004, "BT1 Outdoor Temperature", (*coilInfoSet.find("Current outdoor temperature")).second,
                                     CoilUnit::GradCelcius, CoilDataType::Int16, 10, 0, 0, 0, CoilMode::Read));
}

TEST_CASE("parseNibeModbusCSV, validate only", "[config]") {
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(nibeModbusConfig, nullptr, nullptr));
}

TEST_CASE("parseNibeModbusCSVLine", "[config]") {
    string_deduplicate_t coilInfoSet;
    Coil coil;

    TEST_ASSERT_EQUAL(1, NibeMqttGwConfigManager::parseNibeModbusCSVLine("", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(2, NibeMqttGwConfigManager::parseNibeModbusCSVLine("bad csv", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        1, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"(;"info";40004;"%";s16;10;0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        3, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";XXX;"%";s16;10;0;0;0;R;)", coil, &coilInfoSet));
    // quoting error in info -> id is missing
    TEST_ASSERT_EQUAL(
        3, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info;40004;"%";s16;10;0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        4, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"XX";s16;10;0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        5, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";X16;10;0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        6, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;;0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        7, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;X0;0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        8, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;0;X0;0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        9, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;0;0;X0;R;)", coil, &coilInfoSet));
    TEST_ASSERT_EQUAL(
        10, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;10;0;0;0;X;)", coil, &coilInfoSet));

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;10;0;0;0;R;)", coil,
                                                                              &coilInfoSet));
    const std::string* info = (*coilInfoSet.find("info")).second;
    TEST_ASSERT(coil == Coil(40004, "title", info, CoilUnit::Percent, CoilDataType::Int16, 10, 0, 0, 0, CoilMode::Read));
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;%;u32;1;100;200;1;R/W;)",
                                                                              coil, &coilInfoSet));
    TEST_ASSERT(coil == Coil(40004, "title", info, CoilUnit::Percent, CoilDataType::UInt32, 1, 100, 200, 1, CoilMode::ReadWrite));
}

TEST_CASE("getNextCsvToken", "[config]") {
    std::string token;
    std::stringstream is;

    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is.clear();
    is << "token";
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is.clear();
    is << "token1;token2";
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token1", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token2", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is.clear();
    is << "token1;token2;;";
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token1", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token2", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is.clear();
    is << R"("token1";"token""2";tok"en3";"token;4")";
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token1", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING(R"(token"2)", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING(R"(tok"en3")", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token;4", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));
}

TEST_CASE("parseNibeModbusCSV - nibe-modbus-vvm310.csv", "[config]") {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("cwd: %s\n", cwd);

    std::ifstream is("nibe-modbus-vvm310.csv");
    std::stringstream buffer;
    buffer << is.rdbuf();
    std::string csv = buffer.str();
    TEST_ASSERT(csv.size() > 0);  // check file was found and not empty

    std::unordered_map<uint16_t, Coil> coils;
    string_deduplicate_t coilInfoSet;

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(csv.c_str(), &coils, &coilInfoSet));
    TEST_ASSERT_GREATER_THAN(800, coils.size());
    TEST_ASSERT_GREATER_THAN(100, coilInfoSet.size());
    TEST_ASSERT_LESS_THAN(coils.size(), coilInfoSet.size());

    printf("File contains coils: %lu, info strings: %lu\n", coils.size(), coilInfoSet.size());
}