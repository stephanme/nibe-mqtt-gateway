#include <unistd.h>
#include <unity.h>

#include <fstream>
#include <regex>
#include <sstream>

#include "configmgr.h"

TEST_CASE("default config", "[config]") {
    NibeMqttGwConfigManager configManager;
    const NibeMqttGwConfig& config = configManager.getConfig();
    TEST_ASSERT_EQUAL_STRING("mqtt://mosquitto", config.mqtt.brokerUri.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.mqtt.clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.rootTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("homeassistant", config.mqtt.discoveryPrefix.c_str());
    TEST_ASSERT_EQUAL_STRING("Nibe GW", config.mqtt.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("Nibe", config.mqtt.deviceManufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("Heatpump", config.mqtt.deviceModel.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.mqtt.deviceConfigurationUrl.c_str());
    TEST_ASSERT_EQUAL_STRING("", config.mqtt.hostname.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw/log", config.mqtt.logTopic.c_str());

    TEST_ASSERT_EQUAL(0, config.nibe.registers.size());
    TEST_ASSERT_EQUAL(0, config.nibe.pollRegisters.size());
    TEST_ASSERT_EQUAL(0, config.nibe.pollRegistersSlow.size());
    TEST_ASSERT_EQUAL(0, config.nibe.metrics.size());
    TEST_ASSERT_EQUAL(0, config.nibe.homeassistantDiscoveryOverrides.size());

    TEST_ASSERT_FALSE(config.logging.mqttLoggingEnabled);
    TEST_ASSERT_TRUE(config.logging.stdoutLoggingEnabled);
    TEST_ASSERT_EQUAL_STRING("nibegw/log", config.logging.logTopic.c_str());

    configManager.begin();
    TEST_ASSERT_EQUAL_STRING("nibegw-00:00:00:00:00:00", config.mqtt.clientId.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.rootTopic.c_str());
}

static const char* configJson = R"({
    // can contain comments
    "mqtt": {
        "brokerUri": "mqtt://mosquitto.fritz.box",
        "user": "user",
        "password": "password",
        "rootTopic": "nibegw",
        "discoveryPrefix": "homeassistant",
        "deviceName": "Nibe GW",
        "deviceManufacturer": "Nibe",
        "deviceModel": "VVM 310, S 2125-8"
    },
    "nibe": {
        "pollRegisters": [1,2],
        "pollRegistersSlow": [3,4],
        "metrics": {
            "1": {"name": "prom_name_1{register=\"1\"}", "factor": 10},
            "2": {"name": "prom_name_2{register=\"2\"}"},
            "3": {"name": "prom_name_3{register=\"3\"}", "scale": 10}
        },
        "homeassistantDiscoveryOverrides": {
            "1": {"override1": "value1"},
            "2": {"override2": {"sub2": "value2"}}
        }
    },
    "logging": {
        "mqttLoggingEnabled": true,
        "stdoutLoggingEnabled": true,
        "logTopic": "nibegw/logs",
        "logLevels": {
            "*": "info",
            "mqtt": "debug"
        }
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
    TEST_ASSERT_EQUAL_STRING("Nibe GW", config.mqtt.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING("Nibe", config.mqtt.deviceManufacturer.c_str());
    TEST_ASSERT_EQUAL_STRING("VVM 310, S 2125-8", config.mqtt.deviceModel.c_str());
    TEST_ASSERT_EQUAL_STRING("http://nibegw.fritz.box", config.mqtt.deviceConfigurationUrl.c_str());

    TEST_ASSERT_EQUAL_STRING("nibegw", config.mqtt.hostname.c_str());
    TEST_ASSERT_EQUAL_STRING("nibegw/logs", config.mqtt.logTopic.c_str());
    TEST_ASSERT_EQUAL(2, config.logging.logLevels.size());
    TEST_ASSERT_EQUAL_STRING("info", config.logging.logLevels.at("*").c_str());
    TEST_ASSERT_EQUAL_STRING("debug", config.logging.logLevels.at("mqtt").c_str());

    TEST_ASSERT_EQUAL(2, config.nibe.pollRegisters.size());
    TEST_ASSERT_EQUAL(1, config.nibe.pollRegisters[0]);
    TEST_ASSERT_EQUAL(2, config.nibe.pollRegisters[1]);

    TEST_ASSERT_EQUAL(2, config.nibe.pollRegistersSlow.size());
    TEST_ASSERT_EQUAL(3, config.nibe.pollRegistersSlow[0]);
    TEST_ASSERT_EQUAL(4, config.nibe.pollRegistersSlow[1]);

    TEST_ASSERT_EQUAL(3, config.nibe.metrics.size());
    const NibeRegisterMetricConfig& metric1 = config.nibe.metrics.at(1);
    TEST_ASSERT_EQUAL_STRING(R"(prom_name_1{register="1"})", metric1.name.c_str());
    TEST_ASSERT_EQUAL(10, metric1.factor);
    TEST_ASSERT_EQUAL(0, metric1.scale);
    const NibeRegisterMetricConfig& metric2 = config.nibe.metrics.at(2);
    TEST_ASSERT_EQUAL_STRING(R"(prom_name_2{register="2"})", metric2.name.c_str());
    TEST_ASSERT_EQUAL(0, metric2.factor);
    TEST_ASSERT_EQUAL(0, metric2.scale);
    const NibeRegisterMetricConfig& metric3 = config.nibe.metrics.at(3);
    TEST_ASSERT_EQUAL_STRING(R"(prom_name_3{register="3"})", metric3.name.c_str());
    TEST_ASSERT_EQUAL(0, metric3.factor);
    TEST_ASSERT_EQUAL(10, metric3.scale);

    TEST_ASSERT_EQUAL(2, config.nibe.homeassistantDiscoveryOverrides.size());
    const std::string& override1 = config.nibe.homeassistantDiscoveryOverrides.at(1);
    TEST_ASSERT_EQUAL_STRING(R"({"override1":"value1"})", override1.c_str());
    const std::string& override2 = config.nibe.homeassistantDiscoveryOverrides.at(2);
    TEST_ASSERT_EQUAL_STRING(R"({"override2":{"sub2":"value2"}})", override2.c_str());

    TEST_ASSERT_TRUE(config.logging.mqttLoggingEnabled);
    TEST_ASSERT_TRUE(config.logging.stdoutLoggingEnabled);
    TEST_ASSERT_EQUAL_STRING("nibegw/logs", config.logging.logTopic.c_str());
}

TEST_CASE("getConfigAsJson", "[config]") {
    NibeMqttGwConfigManager configManager;
    configManager.begin();

    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(configJson));
    std::string json = configManager.getConfigAsJson();
    printf("json: %s\n", json.c_str());
    TEST_ASSERT_TRUE(json.contains("mqtt://mosquitto.fritz.box"));
    TEST_ASSERT_TRUE(json.contains("nibegw/logs"));
    TEST_ASSERT_TRUE(json.contains(R"("*": "info")"));
    TEST_ASSERT_TRUE(json.contains(R"("mqtt": "debug")"));
    TEST_ASSERT_TRUE(json.contains("pollRegisters"));
    TEST_ASSERT_TRUE(json.contains("1,"));
    TEST_ASSERT_TRUE(json.contains("pollRegistersSlow"));
    TEST_ASSERT_TRUE(json.contains("3,"));
    TEST_ASSERT_TRUE(json.contains("metrics"));
    TEST_ASSERT_TRUE(json.contains("prom_name_1"));
    TEST_ASSERT_TRUE(json.contains(R"("factor": 10)"));
    TEST_ASSERT_TRUE(json.contains("prom_name_2"));
    TEST_ASSERT_FALSE(json.contains(R"("factor": 0)"));
    TEST_ASSERT_TRUE(json.contains("prom_name_3"));
    TEST_ASSERT_FALSE(json.contains(R"("factor": 0)"));
    TEST_ASSERT_TRUE(json.contains("override1"));
    TEST_ASSERT_TRUE(json.contains("value1"));
    TEST_ASSERT_TRUE(json.contains("override2"));
    TEST_ASSERT_TRUE(json.contains("sub2"));
    TEST_ASSERT_TRUE(json.contains("value2"));

    // test that returned json can be saved again
    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(json.c_str()));
}

TEST_CASE("saveConfig - config.json.template", "[config]") {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("cwd: %s\n", cwd);

    std::ifstream is("config/config.json.template");
    std::stringstream buffer;
    buffer << is.rdbuf();
    std::string json = buffer.str();
    TEST_ASSERT(json.size() > 0);  // check file was found and not empty

    NibeMqttGwConfigManager configManager;
    configManager.begin();
    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(json.c_str()));

    const NibeMqttGwConfig& config = configManager.getConfig();
    TEST_ASSERT_EQUAL_STRING("mqtt://mosquitto.fritz.box", config.mqtt.brokerUri.c_str());
    TEST_ASSERT_GREATER_THAN(0, config.nibe.pollRegisters.size());
    TEST_ASSERT_GREATER_THAN(0, config.nibe.pollRegistersSlow.size());
    TEST_ASSERT_GREATER_THAN(0, config.nibe.metrics.size());
    TEST_ASSERT_GREATER_THAN(0, config.nibe.homeassistantDiscoveryOverrides.size());
}

// ugly concatenation because of ISO-8859-1 for °C
static const char* nibeModbusConfig = R"(ModbusManager 1.0.9
20200624
Product: VVM310, VVM500
Database: 8310
Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
"BT1 Outdoor Temperature";"Current outdoor temperature";40004;")"
                                      "\xB0"
                                      R"(C";s16;10;0;0;0;R;)";

TEST_CASE("parseNibeModbusCSV", "[config]") {
    std::unordered_map<uint16_t, NibeRegister> registers;
    auto is = nonstd::icharbufstream("");
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers));
    std::string badConfig = std::string(nibeModbusConfig);
    badConfig = std::regex_replace(badConfig, std::regex(";Factor;"), ";XFactor;");
    is = nonstd::istringstream(badConfig);
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers));

    is = nonstd::icharbufstream(nibeModbusConfig);
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers));
    TEST_ASSERT_EQUAL(1, registers.size());
    TEST_ASSERT(registers[40004] == NibeRegister(40004, "BT1 Outdoor Temperature", NibeRegisterUnit::GradCelcius,
                                             NibeRegisterDataType::Int16, 10, 0, 0, 0, NibeRegisterMode::Read));

    registers.clear();
    is = nonstd::icharbufstream(nibeModbusConfig);
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers, [](uint16_t id) { return false; }));
    TEST_ASSERT_EQUAL(0, registers.size());

    is = nonstd::icharbufstream(nibeModbusConfig);
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers, [](uint16_t id) { return id == 40004; }));
    TEST_ASSERT_EQUAL(1, registers.size());
}

TEST_CASE("parseNibeModbusCSV, validate only", "[config]") {
    auto is = nonstd::icharbufstream(nibeModbusConfig);
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(is, nullptr));
}

TEST_CASE("parseNibeModbusCSVLine", "[config]") {
    NibeRegister _register;

    TEST_ASSERT_EQUAL(1, NibeMqttGwConfigManager::parseNibeModbusCSVLine("", _register));
    TEST_ASSERT_EQUAL(2, NibeMqttGwConfigManager::parseNibeModbusCSVLine("bad csv", _register));
    TEST_ASSERT_EQUAL(1, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"(;"info";40004;"%";s16;10;0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(3, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";XXX;"%";s16;10;0;0;0;R;)", _register));
    // quoting error in info -> id is missing
    TEST_ASSERT_EQUAL(3, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info;40004;"%";s16;10;0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(4, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"XX";s16;10;0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(5, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";X16;10;0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(6, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;;0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(7, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;X0;0;0;R;)", _register));
    TEST_ASSERT_EQUAL(8, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;0;X0;0;R;)", _register));
    TEST_ASSERT_EQUAL(9, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;1;0;0;X0;R;)", _register));
    TEST_ASSERT_EQUAL(10, NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;10;0;0;0;X;)", _register));

    TEST_ASSERT_EQUAL(ESP_OK,
                      NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;"%";s16;10;0;0;0;R;)", _register));
    TEST_ASSERT(_register == NibeRegister(40004, "title", NibeRegisterUnit::Percent, NibeRegisterDataType::Int16, 10, 0, 0, 0,
                                     NibeRegisterMode::Read));
    TEST_ASSERT_EQUAL(ESP_OK,
                      NibeMqttGwConfigManager::parseNibeModbusCSVLine(R"("title";"info";40004;%;u32;1;100;200;1;R/W;)", _register));
    TEST_ASSERT(_register == NibeRegister(40004, "title", NibeRegisterUnit::Percent, NibeRegisterDataType::UInt32, 1, 100, 200, 1,
                                     NibeRegisterMode::ReadWrite));
}

TEST_CASE("getNextCsvToken", "[config]") {
    std::string token;
    nonstd::icharbufstream is("");

    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is = nonstd::icharbufstream("token");
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is = nonstd::icharbufstream("token1;token2");
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token1", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token2", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is = nonstd::icharbufstream("token1;token2;;");
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token1", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("token2", token.c_str());
    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::getNextCsvToken(is, token));
    TEST_ASSERT_EQUAL_STRING("", token.c_str());
    TEST_ASSERT_EQUAL(ESP_FAIL, NibeMqttGwConfigManager::getNextCsvToken(is, token));

    is = nonstd::icharbufstream(R"("token1";"token""2";tok"en3";"token;4")");
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

    std::ifstream ifs("config/nibe-modbus-vvm310.csv");
    nonstd::istdstream is(ifs);
    std::unordered_map<uint16_t, NibeRegister> registers;

    TEST_ASSERT_EQUAL(ESP_OK, NibeMqttGwConfigManager::parseNibeModbusCSV(is, &registers));
    TEST_ASSERT_GREATER_THAN(800, registers.size());

    printf("nibe-modbus-vvm310.csv contains %lu registers\n", registers.size());
}

TEST_CASE("parseNibeModbusCSV - nibe-modbus-vvm310.csv, filter configured registers", "[config]") {
    std::ifstream ifs("config/config.json.template");
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string json = buffer.str();
    TEST_ASSERT(json.size() > 0);  // check file was found and not empty
    ifs.close();
    ifs.clear();

    NibeMqttGwConfigManager configManager;
    configManager.begin();
    TEST_ASSERT_EQUAL(ESP_OK, configManager.saveConfig(json.c_str()));

    ifs.open("config/nibe-modbus-vvm310.csv");
    nonstd::istdstream is(ifs);
    std::unordered_map<uint16_t, NibeRegister> registers;

    TEST_ASSERT_EQUAL(
        ESP_OK,
        NibeMqttGwConfigManager::parseNibeModbusCSV(
            is, &registers, std::bind(&NibeMqttGwConfigManager::nibeRegisterFilterConfigured, configManager, std::placeholders::_1)));
    TEST_ASSERT_GREATER_THAN(20, registers.size());
    TEST_ASSERT_LESS_THAN(50, registers.size());

    printf("config.json.template references %lu registers\n", registers.size());
}