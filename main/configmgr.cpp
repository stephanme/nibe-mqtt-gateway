#include "configmgr.h"

#include <ArduinoJson.h>
#include <esp_log.h>

#include <cstring>
#include <istream>
#include <streambuf>

#if CONFIG_IDF_TARGET_LINUX
#else
#include <ETH.h>
#endif

// streambuf that operates on passed in data (no memory allocation)
struct externbuf : public std::streambuf {
    externbuf(const char* data, unsigned int len) : begin(data), crt(data), end(data + len) {}

    int_type underflow() { return crt == end ? traits_type::eof() : traits_type::to_int_type(*crt); }
    int_type uflow() { return crt == end ? traits_type::eof() : traits_type::to_int_type(*crt++); }
    int_type pbackfail(int_type ch) {
        bool cond = crt == begin || (ch != traits_type::eof() && ch != crt[-1]);
        return cond ? traits_type::eof() : traits_type::to_int_type(*--crt);
    }
    std::streamsize showmanyc() { return end - crt; }

    const char *begin, *crt, *end;
};

static const char* TAG = "config";

#define CONFIG_FILE "/config.json"
#define NIBE_MODBUS_FILE "/nibe_modbus.csv"

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
                .hostname = "",
                .logTopic = "nibegw/log",
            },
        .nibe =
            {
                .coils = {},
                .coilsToPoll = {},
            },
        .logging =
            {
                .mqttLoggingEnabled = false,
                .stdoutLoggingEnabled = true,
                .logTopic = "nibegw/log",
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
    File file = LittleFS.open(CONFIG_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading config file %s", CONFIG_FILE);

        // TODO: avoid String copies
        String json = file.readString();
        file.close();
        NibeMqttGwConfig tmpConfig;
        if (parseJson(json.c_str(), tmpConfig) != ESP_OK) {
            return ESP_FAIL;
        }
        // use valid config
        config = tmpConfig;
    } else {
        ESP_LOGW(TAG, "Config file %s not found", CONFIG_FILE);
    }

    file = LittleFS.open(NIBE_MODBUS_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading nibe modbus config file %s", NIBE_MODBUS_FILE);
        ArduinoStream as(file);
        std::istream is(&as);
        if (parseNibeModbusCSV(is, &config.nibe.coils) != ESP_OK) {
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

const std::string NibeMqttGwConfigManager::getConfigAsJson() {
    JsonDocument doc;
    doc["mqtt"]["brokerUri"] = config.mqtt.brokerUri;
    doc["mqtt"]["user"] = config.mqtt.user;
    doc["mqtt"]["password"] = config.mqtt.password;
    doc["mqtt"]["clientId"] = config.mqtt.clientId;
    doc["mqtt"]["rootTopic"] = config.mqtt.rootTopic;
    doc["mqtt"]["discoveryPrefix"] = config.mqtt.discoveryPrefix;

    JsonArray coilsToPoss = doc["nibe"]["coilsToPoll"].to<JsonArray>();
    for (auto coil : config.nibe.coilsToPoll) {
        coilsToPoss.add(coil);
    }
    JsonObject metrics = doc["nibe"]["metrics"].to<JsonObject>();
    for (auto [id, metric] : config.nibe.metrics) {
        JsonObject m = metrics[std::to_string(id)].to<JsonObject>();
        m["name"] = metric.name;
        if (metric.factor != 0) m["factor"] = metric.factor;
    }

    doc["logging"]["mqttLoggingEnabled"] = config.logging.mqttLoggingEnabled;
    doc["logging"]["stdoutLoggingEnabled"] = config.logging.stdoutLoggingEnabled;
    doc["logging"]["logTopic"] = config.logging.logTopic;

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

    for (auto coil : doc["nibe"]["coilsToPoll"].as<JsonArray>()) {
        config.nibe.coilsToPoll.push_back(coil.as<uint16_t>());
    }

    for (auto metric : doc["nibe"]["metrics"].as<JsonObject>()) {
        uint16_t id = atoi(metric.key().c_str());
        if (id > 0) {
            config.nibe.metrics[id] = {
                .name = metric.value()["name"] | "",
                .factor = metric.value()["factor"].as<int>() | 0,
            };
        } else {
            // log and skip
            ESP_LOGE(TAG, "nibe.metrics: invalid coil address %s", metric.key().c_str());
        }
    }

    config.logging.mqttLoggingEnabled = doc["logging"]["mqttLoggingEnabled"] | false;
    config.logging.stdoutLoggingEnabled = doc["logging"]["stdoutLoggingEnabled"] | true;
    config.logging.logTopic = doc["logging"]["logTopic"] | "nibegw/log";

    config.mqtt.logTopic = config.logging.logTopic;

    return ESP_OK;
}

const std::string NibeMqttGwConfigManager::getNibeModbusConfig() {
    // TODO: return nibe csv file from FS
    std::string csv;

#if CONFIG_IDF_TARGET_LINUX
    ESP_LOGW(TAG, "Skip writing config on Linux target");
    csv = "not supported on linux target";
#else
    File file = LittleFS.open(NIBE_MODBUS_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading nibe modbus config file %s", NIBE_MODBUS_FILE);

        ArduinoStream as(file);
        std::istream is(&as);
        csv = std::string(std::istreambuf_iterator<char>(is), {});
        file.close();
    } else {
        ESP_LOGW(TAG, "Nibe modbus config file %s not found", NIBE_MODBUS_FILE);
    }
#endif

    return csv;
}

esp_err_t NibeMqttGwConfigManager::saveNibeModbusConfig(const char* uploadFileName) {
#if CONFIG_IDF_TARGET_LINUX
    // uploadFileName = csv - only for testing
    std::unordered_map<uint16_t, Coil> tmpCoils;
    if (parseNibeModbusCSV(uploadFileName, &tmpCoils) != ESP_OK) {
        return ESP_FAIL;
    }
    ESP_LOGW(TAG, "Skip writing config on Linux target");
    // store for testing
    config.nibe.coils = tmpCoils;
#else
    // validate csv
    File file = LittleFS.open(uploadFileName);
    if (!file) {
        ESP_LOGE(TAG, "Failed to open nibe modbus config file %s", uploadFileName);
        return ESP_FAIL;
    }
    ArduinoStream as(file);
    std::istream is(&as);
    if (parseNibeModbusCSV(is, nullptr) != ESP_OK) {
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

esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSV(const char* csv, std::unordered_map<uint16_t, Coil>* coils) {
    externbuf buf(csv, strlen(csv));
    std::istream is(&buf);
    return parseNibeModbusCSV(is, coils);
}

// Nibe ModbusManager CSV format:
// ModbusManager 1.0.9
// 20200624
// Product: VVM310, VVM500
// Database: 8310
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
// "BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
//
// if coils is null, the input is only checked for format
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSV(std::istream& is, std::unordered_map<uint16_t, Coil>* coils) {
    std::string line;
    line.reserve(256);
    int line_num = 0;

    // eat header and check format
    if (!is) {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    std::getline(is, line);
    line_num++;
    if (!line.starts_with("ModbusManager")) {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    for (int i = 0; i < 3; i++) {
        std::getline(is, line);
        line_num++;
        if (!is) {
            ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
            return ESP_FAIL;
        }
    }
    std::getline(is, line);
    line_num++;
    if (!is || line != "Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode") {
        ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Bad header", line_num);
        return ESP_FAIL;
    }
    // read coil configuration
    while (is) {
        std::getline(is, line);
        line_num++;
        // if (line_num % 50 == 0) {
        //     ESP_LOGI(TAG, "Nibe Modbus CSV, line %d, minHeap %lu", line_num, ESP.getMinFreeHeap());
        // }
        if (line.empty()) {
            continue;
        }
        Coil coil;
        esp_err_t err = parseNibeModbusCSVLine(line, coil);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Format error in token #%d: %s", line_num, err, line.c_str());
            return ESP_FAIL;
        }
        if (coils != nullptr) {
            coils->operator[](coil.id) = coil;
        }
    }
    return ESP_OK;
}

// Format:
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
// "BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
//
// returns ESP_OK if correctly parsed and coil is valid or the number of the bad token
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSVLine(const std::string& line, Coil& coil) {
    externbuf buf(line.c_str(), line.size());
    std::istream is(&buf);
    std::string token;
    esp_err_t token_num = 1;  // returned as err msg
    try {
        // Title (mandatory)
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        if (token.empty()) return token_num;
        coil.title = token;
        token_num++;
        // Info (optional), ignored
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        token_num++;
        // ID
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        if (token.empty()) return token_num;
        coil.id = std::stoi(token);
        token_num++;
        // Unit
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.unit = Coil::stringToUnit(token.c_str());
        if (coil.unit == CoilUnit::Unknown) return token_num;
        token_num++;
        // Size
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.dataType = nibeModbusSizeToDataType(token);
        if (coil.dataType == CoilDataType::Unknown) return token_num;
        token_num++;
        // Factor
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.factor = std::stoi(token);
        token_num++;
        // Min
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.minValue = std::stoi(token);
        token_num++;
        // Max
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.maxValue = std::stoi(token);
        token_num++;
        // Default
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.defaultValue = std::stoi(token);
        token_num++;
        // Mode
        if (getNextCsvToken(is, token) != ESP_OK) return token_num;
        coil.mode = nibeModbusMode(token);
        if (coil.mode == CoilMode::Unknown) return token_num;

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
esp_err_t NibeMqttGwConfigManager::getNextCsvToken(std::istream& is, std::string& token) {
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

CoilDataType NibeMqttGwConfigManager::nibeModbusSizeToDataType(const std::string& size) {
    // TODO: more data types like date, needs additional metadata because not encoded in CSV
    if (size == "s8") return CoilDataType::Int8;
    if (size == "s16") return CoilDataType::Int16;
    if (size == "s32") return CoilDataType::Int32;
    if (size == "u8") return CoilDataType::UInt8;
    if (size == "u16") return CoilDataType::UInt16;
    if (size == "u32") return CoilDataType::UInt32;
    return CoilDataType::Unknown;
}

CoilMode NibeMqttGwConfigManager::nibeModbusMode(const std::string& mode) {
    if (mode == "R") return CoilMode::Read;
    if (mode == "W") return CoilMode::Write;
    if (mode == "R/W") return CoilMode::ReadWrite;
    return CoilMode::Unknown;
}