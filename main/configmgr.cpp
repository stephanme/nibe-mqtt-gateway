#include "configmgr.h"

#include <ArduinoJson.h>

#include <cstring>

#if CONFIG_IDF_TARGET_LINUX
#else
#include <ETH.h>
#endif

static const char* TAG = "config";

#define CONFIG_FILE "/config.json"

NibeMqttGwConfigManager::NibeMqttGwConfigManager() {
    // initialize with default values
    config = {
        .mqtt =
            {
                .brokerUri = "mqtt://mosquitto",
                .user = "",
                .password = "",
                .clientId = "",
                .rootTopic = "nibegw",
                .discoveryPrefix = "homeassistant",
                .deviceName = "Nibe GW",
                .deviceManufacturer = "Nibe",
                .deviceModel = "Heatpump",
                .deviceConfigurationUrl = "",
                .hostname = "",
                .logTopic = "nibegw/log",
            },
        .nibe =
            {
                .coils = {},
                .coilsToPoll = {},
                .coilsToPollLowFrequency = {},
                .metrics = {},
                .homeassistantDiscoveryOverrides = {},
            },
        .logging =
            {
                .mqttLoggingEnabled = false,
                .stdoutLoggingEnabled = true,
                .logTopic = "nibegw/log",
                .logLevels = {},
            },
    };
}

esp_err_t NibeMqttGwConfigManager::begin() {
#if CONFIG_IDF_TARGET_LINUX
    hostname = "nibegw";
    defaultClientId = "nibegw-00:00:00:00:00:00";
    config.mqtt.hostname = hostname;
    config.mqtt.clientId = defaultClientId;

    ESP_LOGW(TAG, "Skip loading config on Linux target");
#else
    hostname = ETH.getHostname();
    defaultClientId = hostname;
    defaultClientId += "-";
    defaultClientId += ETH.macAddress().c_str();
    config.mqtt.hostname = hostname;
    config.mqtt.clientId = defaultClientId;

    if (!LittleFS.begin(true)) {
        ESP_LOGE(TAG, "Failed to mount file system");
        return ESP_FAIL;
    }

    std::string configJson = getConfigAsJson();
    if (!configJson.empty()) {
        NibeMqttGwConfig tmpConfig;
        if (parseJson(configJson.c_str(), tmpConfig) != ESP_OK) {
            return ESP_FAIL;
        }
        // use valid config
        config = tmpConfig;
        // configure log levels
        setLogLevels(config.logging.logLevels);
    } else {
        ESP_LOGW(TAG, "Config file %s not found", CONFIG_FILE);
    }

    File file = LittleFS.open(NIBE_MODBUS_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading nibe modbus config file %s", NIBE_MODBUS_FILE);
        nonstd::arduinostream is(file);
        // filter for coils that are specified in config to poll, as metrics or for HA discovery
        if (parseNibeModbusCSV(is, &config.nibe.coils,
                               std::bind(&NibeMqttGwConfigManager::nibeRegisterFilterConfigured, this, std::placeholders::_1)) !=
            ESP_OK) {
            file.close();
            return ESP_FAIL;
        }
        file.close();
    } else {
        ESP_LOGW(TAG, "Nibe modbus config file %s not found", NIBE_MODBUS_FILE);
    }
#endif

    return ESP_OK;
}

bool NibeMqttGwConfigManager::nibeRegisterFilterConfigured(u_int16_t id) const {
    // filter for coils that are specified in config to poll, as metrics or for HA discovery
    auto coilsToPollEnd = this->config.nibe.coilsToPoll.end();
    auto coilsToPollLFEnd = this->config.nibe.coilsToPollLowFrequency.end();
    return std::find(this->config.nibe.coilsToPoll.begin(), coilsToPollEnd, id) != coilsToPollEnd ||
           std::find(this->config.nibe.coilsToPollLowFrequency.begin(), coilsToPollLFEnd, id) != coilsToPollLFEnd ||
           this->config.nibe.metrics.find(id) != this->config.nibe.metrics.end() ||
           this->config.nibe.homeassistantDiscoveryOverrides.find(id) != this->config.nibe.homeassistantDiscoveryOverrides.end();
}

// returns config file as uploaded (i.e. including comments)
const std::string NibeMqttGwConfigManager::getConfigAsJson() {
#if CONFIG_IDF_TARGET_LINUX
    return getRuntimeConfigAsJson();
#else
    File file = LittleFS.open(CONFIG_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading config file %s", CONFIG_FILE);

        // TODO: avoid String copies
        String json = file.readString();
        file.close();
        return json.c_str();
    }
    return "";
#endif
}

const std::string NibeMqttGwConfigManager::getRuntimeConfigAsJson() {
    JsonDocument doc;
    doc["mqtt"]["brokerUri"] = config.mqtt.brokerUri;
    doc["mqtt"]["user"] = config.mqtt.user;
    doc["mqtt"]["password"] = config.mqtt.password;
    doc["mqtt"]["clientId"] = config.mqtt.clientId;
    doc["mqtt"]["rootTopic"] = config.mqtt.rootTopic;
    doc["mqtt"]["discoveryPrefix"] = config.mqtt.discoveryPrefix;
    doc["mqtt"]["deviceName"] = config.mqtt.deviceName;
    doc["mqtt"]["deviceModel"] = config.mqtt.deviceModel;
    doc["mqtt"]["deviceManufacturer"] = config.mqtt.deviceManufacturer;
    doc["mqtt"]["deviceConfigurationUrl"] = config.mqtt.deviceConfigurationUrl;

    JsonArray coilsToPoll = doc["nibe"]["coilsToPoll"].to<JsonArray>();
    for (auto coil : config.nibe.coilsToPoll) {
        coilsToPoll.add(coil);
    }
    JsonArray coilsToPollLowFrequency = doc["nibe"]["coilsToPollLowFrequency"].to<JsonArray>();
    for (auto coil : config.nibe.coilsToPollLowFrequency) {
        coilsToPollLowFrequency.add(coil);
    }
    JsonObject metrics = doc["nibe"]["metrics"].to<JsonObject>();
    for (auto [id, metric] : config.nibe.metrics) {
        JsonObject m = metrics[std::to_string(id)].to<JsonObject>();
        m["name"] = metric.name;
        if (metric.factor != 0) m["factor"] = metric.factor;
        if (metric.scale != 0) m["scale"] = metric.scale;
    }
    JsonObject homeassistantDiscoveryOverrides = doc["nibe"]["homeassistantDiscoveryOverrides"].to<JsonObject>();
    for (auto [id, override] : config.nibe.homeassistantDiscoveryOverrides) {
        JsonObject o = homeassistantDiscoveryOverrides[std::to_string(id)].to<JsonObject>();
        DeserializationError err = deserializeJson(o, override);
        if (err) {
            ESP_LOGE(TAG, "deserializeJson() of homeassistantDiscoveryOverrides[%u] failed: %s", id, err.c_str());
        }
    }

    doc["logging"]["mqttLoggingEnabled"] = config.logging.mqttLoggingEnabled;
    doc["logging"]["stdoutLoggingEnabled"] = config.logging.stdoutLoggingEnabled;
    doc["logging"]["logTopic"] = config.logging.logTopic;
    JsonObject logLevels = doc["logging"]["logLevels"].to<JsonObject>();
    for (auto [tag, level] : config.logging.logLevels) {
        logLevels[tag] = level;
    }

    std::string json;
    serializeJsonPretty(doc, json);
    return json;
}

esp_err_t NibeMqttGwConfigManager::saveConfig(const char* configJson) {
    // validate config json
    NibeMqttGwConfig tmpConfig;
    if (parseJson(configJson, tmpConfig) != ESP_OK) {
        return ESP_FAIL;
    }

#if CONFIG_IDF_TARGET_LINUX
    ESP_LOGW(TAG, "Skip writing config on Linux target");
    // store for testing
    config = tmpConfig;
#else
    // write to file
    File file = LittleFS.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        ESP_LOGE(TAG, "Failed to write config file %s", CONFIG_FILE);
        return ESP_FAIL;
    }
    if (!file.print(configJson)) {
        ESP_LOGE(TAG, "Failed to write config file %s", CONFIG_FILE);
        file.close();
        return ESP_FAIL;
    }

    file.close();
#endif
    return ESP_OK;
}

esp_err_t NibeMqttGwConfigManager::parseJson(const char* jsonString, NibeMqttGwConfig& config) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        ESP_LOGE(TAG, "deserializeJson() failed: %s", error.c_str());
        return ESP_FAIL;
    }
    config.mqtt.brokerUri = doc["mqtt"]["brokerUri"].as<std::string>();
    if (config.mqtt.brokerUri.empty()) {
        ESP_LOGE(TAG, "brokerUri is required");
        return ESP_FAIL;
    }
    config.mqtt.user = doc["mqtt"]["user"].as<std::string>();
    config.mqtt.password = doc["mqtt"]["password"].as<std::string>();
    config.mqtt.clientId = doc["mqtt"]["clientId"] | defaultClientId;
    config.mqtt.rootTopic = doc["mqtt"]["rootTopic"] | "nibegw";
    config.mqtt.discoveryPrefix = doc["mqtt"]["discoveryPrefix"] | "homeassistant";
    config.mqtt.hostname = hostname;
    config.mqtt.deviceName = doc["mqtt"]["deviceName"] | "Nibe GW";
    config.mqtt.deviceModel = doc["mqtt"]["deviceModel"] | "Heatpump";
    config.mqtt.deviceManufacturer = doc["mqtt"]["deviceManufacturer"] | "Nibe";
    config.mqtt.deviceConfigurationUrl = doc["mqtt"]["deviceConfigurationUrl"] | "";
    if (config.mqtt.deviceConfigurationUrl.empty()) {
        char defaultConfigUrl[128];
        snprintf(defaultConfigUrl, sizeof(defaultConfigUrl), "http://%s.fritz.box", hostname.c_str());
        config.mqtt.deviceConfigurationUrl = defaultConfigUrl;
    }

    for (auto coil : doc["nibe"]["coilsToPoll"].as<JsonArray>()) {
        config.nibe.coilsToPoll.push_back(coil.as<uint16_t>());
    }
    for (auto coil : doc["nibe"]["coilsToPollLowFrequency"].as<JsonArray>()) {
        config.nibe.coilsToPollLowFrequency.push_back(coil.as<uint16_t>());
    }

    for (auto metric : doc["nibe"]["metrics"].as<JsonObject>()) {
        uint16_t id = atoi(metric.key().c_str());
        if (id > 0) {
            config.nibe.metrics[id] = {
                .name = metric.value()["name"] | "",
                .factor = metric.value()["factor"].as<int>() | 0,
                .scale = metric.value()["scale"].as<int>() | 0,
                .counter = metric.value()["counter"] | false,
            };
        } else {
            // log and skip
            ESP_LOGE(TAG, "nibe.metrics: invalid coil address %s", metric.key().c_str());
        }
    }

    for (auto override : doc["nibe"]["homeassistantDiscoveryOverrides"].as<JsonObject>()) {
        uint16_t id = atoi(override.key().c_str());
        if (id > 0) {
            std::string overrideJson;
            serializeJson(override.value(), overrideJson);
            config.nibe.homeassistantDiscoveryOverrides[id] = overrideJson;
        } else {
            // log and skip
            ESP_LOGE(TAG, "nibe.homeassistantDiscoveryOverrides: invalid coil address %s", override.key().c_str());
        }
    }

    config.logging.mqttLoggingEnabled = doc["logging"]["mqttLoggingEnabled"] | false;
    config.logging.stdoutLoggingEnabled = doc["logging"]["stdoutLoggingEnabled"] | true;
    config.logging.logTopic = doc["logging"]["logTopic"] | "nibegw/log";
    for (auto metric : doc["logging"]["logLevels"].as<JsonObject>()) {
        config.logging.logLevels[metric.key().c_str()] = metric.value().as<std::string>();
    }

    config.mqtt.logTopic = config.logging.logTopic;

    return ESP_OK;
}

esp_err_t NibeMqttGwConfigManager::saveNibeModbusConfig(const char* uploadFileName) {
#if CONFIG_IDF_TARGET_LINUX
    // uploadFileName = csv - only for testing
    std::unordered_map<uint16_t, NibeRegister> tmpNibeRegisters;
    auto is = nonstd::icharbufstream(uploadFileName);
    if (parseNibeModbusCSV(is, &tmpNibeRegisters) != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGW(TAG, "Skip writing config on Linux target");
    // store for testing
    config.nibe.coils = tmpNibeRegisters;
#else
    // validate csv
    File file = LittleFS.open(uploadFileName);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open nibe modbus config file %s", uploadFileName);
        return ESP_FAIL;
    }
    nonstd::arduinostream is(file);
    if (parseNibeModbusCSV(is) != ESP_OK) {
        file.close();
        return ESP_FAIL;
    }
    file.close();

    // delete old config and rename upload file
    if (!LittleFS.remove(NIBE_MODBUS_FILE) || !LittleFS.rename(uploadFileName, NIBE_MODBUS_FILE)) {
        ESP_LOGE(TAG, "Failed to save nibe modbus config file %s", NIBE_MODBUS_FILE);
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

// Nibe ModbusManager CSV format:
// ModbusManager 1.0.9
// 20200624
// Product: VVM310, VVM500
// Database: 8310
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
// "BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
//
// if registers is null, the input is only checked for format
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSV(nonstd::istream& is, std::unordered_map<uint16_t, NibeRegister>* registers,
                                                      nibeRegisterFilterFunction_t filter) {
    std::string line;
    line.reserve(256);
    int line_num = 0;

    // eat header and check format
    if (!is) {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    nonstd::getline(is, line);
    line_num++;
    if (!line.starts_with("ModbusManager")) {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    for (int i = 0; i < 3; i++) {
        nonstd::getline(is, line);
        line_num++;
        if (!is) {
            ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
            return ESP_FAIL;
        }
    }
    nonstd::getline(is, line);
    line_num++;
    if (!is || line != "Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode") {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    // read register configuration
    while (is) {
        nonstd::getline(is, line);
        line_num++;
        // if (line_num % 50 == 0) {
        //     ESP_LOGI(TAG, "Nibe Modbus CSV, line %d, minHeap %lu", line_num, ESP.getMinFreeHeap());
        // }
        if (line.empty()) {
            continue;
        }
        NibeRegister _register;
        esp_err_t err = parseNibeModbusCSVLine(line, _register);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Format error in token #%d: %s", line_num, err, line.c_str());
            return ESP_FAIL;
        }
        if (registers != nullptr && filter(_register.id)) {
            registers->operator[](_register.id) = _register;
        }
    }
    return ESP_OK;
}

// Format:
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
// "BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
//
// returns ESP_OK if correctly parsed and register is valid or the number of the bad token
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSVLine(const std::string& line, NibeRegister& _register) {
    auto is = nonstd::istringstream(line);
    std::string token;
    esp_err_t token_num = 1;  // returned as err msg
    try {
        // Title (mandatory)
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        if (token.empty()) return token_num;
        _register.title = token;
        token_num++;
        // Info (optional), ignored
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        token_num++;
        // ID
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        if (token.empty()) return token_num;
        _register.id = std::stoi(token);
        token_num++;
        // Unit
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.unit = NibeRegister::stringToUnit(token.c_str());
        if (_register.unit == NibeRegisterUnit::Unknown) return token_num;
        token_num++;
        // Size
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.dataType = nibeModbusSizeToDataType(token);
        if (_register.dataType == NibeRegisterDataType::Unknown) return token_num;
        token_num++;
        // Factor
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.factor = std::stoi(token);
        token_num++;
        // Min
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.minValue = std::stoi(token);
        token_num++;
        // Max
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.maxValue = std::stoi(token);
        token_num++;
        // Default
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.defaultValue = std::stoi(token);
        token_num++;
        // Mode
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        _register.mode = nibeModbusMode(token);
        if (_register.mode == NibeRegisterMode::Unknown) return token_num;

        return ESP_OK;
#if CONFIG_IDF_TARGET_LINUX
    } catch (...) {
        // for linux only to get tests green
        // don't use on esp32 because it hides OOMs
        return token_num;
    }
#else
    } catch (const std::invalid_argument& e) {
        // TODO: is not caught on linux but works on esp32, unclear why
        return token_num;
    }
#endif
}

// Reads next CSV token and stores it in 'token', reads ; separator
// handles quoting
esp_err_t NibeMqttGwConfigManager::getNextCsvToken(nonstd::istream& is, std::string& token) {
    token.clear();

    bool quoted = false;
    int c = is.get();
    if (c == EOF) {
        return ESP_FAIL;
    }
    if (c == '"') {
        quoted = true;
        c = is.get();
    }
    while (1) {
        if (quoted) {
            if (c == EOF) {
                return ESP_FAIL;
            } else if (c == '"') {
                c = is.get();
                if (c == '"') {
                    // "" within quoted string
                    token.push_back(c);
                    c = is.get();
                } else {
                    quoted = false;
                }
            } else {
                token.push_back(c);
                c = is.get();
            }
        } else {
            if (c == ';' || c == EOF) {
                return ESP_OK;
            } else {
                token.push_back(c);
                c = is.get();
            }
        }
    }
    return ESP_OK;
}

NibeRegisterDataType NibeMqttGwConfigManager::nibeModbusSizeToDataType(const std::string& size) {
    if (size == "s8") return NibeRegisterDataType::Int8;
    if (size == "s16") return NibeRegisterDataType::Int16;
    if (size == "s32") return NibeRegisterDataType::Int32;
    if (size == "u8") return NibeRegisterDataType::UInt8;
    if (size == "u16") return NibeRegisterDataType::UInt16;
    if (size == "u32") return NibeRegisterDataType::UInt32;
    return NibeRegisterDataType::Unknown;
}

NibeRegisterMode NibeMqttGwConfigManager::nibeModbusMode(const std::string& mode) {
    if (mode == "R") return NibeRegisterMode::Read;
    if (mode == "W") return NibeRegisterMode::Write;
    if (mode == "R/W") return NibeRegisterMode::ReadWrite;
    return NibeRegisterMode::Unknown;
}

void NibeMqttGwConfigManager::setLogLevels(const std::unordered_map<std::string, std::string>& logLevels) {
    auto defLogLevel = logLevels.find("*");
    if (defLogLevel != logLevels.end()) {
        esp_log_level_set("*", toLogLevel(defLogLevel->second));
    }
    for (auto [tag, level] : logLevels) {
        if (tag != "*") {
            esp_log_level_set(tag.c_str(), toLogLevel(level));
        }
    }
}

esp_log_level_t NibeMqttGwConfigManager::toLogLevel(const std::string& level) {
    if (level == "verbose") {
        return ESP_LOG_VERBOSE;
    } else if (level == "debug") {
        return ESP_LOG_DEBUG;
    } else if (level == "info") {
        return ESP_LOG_INFO;
    } else if (level == "warn") {
        return ESP_LOG_WARN;
    } else if (level == "error") {
        return ESP_LOG_ERROR;
    }
    return ESP_LOG_NONE;
}
