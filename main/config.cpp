#include "config.h"

#include <ArduinoJson.h>
#include <esp_log.h>

#include <istream>
#include <streambuf>

#if CONFIG_IDF_TARGET_LINUX
#else
#include <ETH.h>
#include <LittleFS.h>

// adapter to use std::stream with Arduino Stream
// implements reading only
// see https://en.cppreference.com/w/cpp/io/basic_streambuf/underflow
class ArduinoStream : public std::streambuf {
    Stream& stream;
    char ch;

   public:
    ArduinoStream(Stream& stream) : stream(stream) {
        setg(&ch, &ch + 1, &ch + 1);  // buffer is initially full (= nothing to read)
    }

    int_type underflow() override {
        if (stream.available() > 0) {
            ch = stream.read();
            setg(&ch, &ch, &ch + 1);  // make one read position available
            return ch;
        }
        return traits_type::eof();
    }
};
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
        if (parseNibeModbusCSV(is, config.nibe.coils) != ESP_OK) {
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
    // store for testing
    csv = R"(ModbusManager 1.0.9
        20200624
        Product: VVM310, VVM500
        Database: 8310
        Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
        )";
    for (auto [id, coil] : config.nibe.coils) {
        csv += "\"" + coil.title + "\";\"" + coil.info + "\";" + std::to_string(coil.id) + ";\"" + coil.unit + "\";";
        switch (coil.dataType) {
            case COIL_DATA_TYPE_INT8:
                csv += "s8;";
                break;
            case COIL_DATA_TYPE_INT16:
                csv += "s16;";
                break;
            case COIL_DATA_TYPE_INT32:
                csv += "s32;";
                break;
            case COIL_DATA_TYPE_UINT8:
                csv += "u8;";
                break;
            case COIL_DATA_TYPE_UINT16:
                csv += "u16;";
                break;
            case COIL_DATA_TYPE_UINT32:
                csv += "u32;";
                break;
            default:
                csv += "unknown;";
                break;
        }
        csv += std::to_string(coil.factor) + ";" + std::to_string(coil.minValue) + ";" + std::to_string(coil.maxValue) + ";" +
               std::to_string(coil.defaultValue) + ";";
        switch (coil.mode) {
            case COIL_MODE_READ:
                csv += "R;";
                break;
            case COIL_MODE_WRITE:
                csv += "W;";
                break;
            case COIL_MODE_READ_WRITE:
                csv += "RW;";
                break;
            default:
                csv += "unknown;";
                break;
        }
        csv += "\n";
    }
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

esp_err_t NibeMqttGwConfigManager::saveNibeModbusConfig(const char* csv) {
    std::unordered_map<uint16_t, Coil> tmpCoils;
    if (parseNibeModbusCSV(csv, tmpCoils) != ESP_OK) {
        return ESP_FAIL;
    }

#if CONFIG_IDF_TARGET_LINUX
    ESP_LOGW(TAG, "Skip writing config on Linux target");
    // store for testing
    config.nibe.coils = tmpCoils;
#else
    // write to file
    File file = LittleFS.open(NIBE_MODBUS_FILE, FILE_WRITE);
    if (!file) {
        ESP_LOGE(TAG, "Failed to nibe modbus config file %s", NIBE_MODBUS_FILE);
        return ESP_FAIL;
    }
    if (!file.print(csv)) {
        ESP_LOGE(TAG, "Failed to write nibe modbus config file %s", NIBE_MODBUS_FILE);
        file.close();
        return ESP_FAIL;
    }

    file.close();
#endif

    return ESP_OK;
}

esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSV(const char* csv, std::unordered_map<uint16_t, Coil>& coils) {
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
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSV(std::istream& is, std::unordered_map<uint16_t, Coil>& coils) {
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
        if (line.empty()) {
            continue;
        }
        Coil coil;
        if (parseNibeModbusCSVLine(line, coil) != ESP_OK) {
            ESP_LOGE(TAG, "Nibe Modbus CSV, line %d: Format error: %s", line_num, line.c_str());
            return ESP_FAIL;
        }
        coils[coil.id] = coil;
    }
    return ESP_OK;
}

// Format:
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
// "BT1 Outdoor Temperature";"Current outdoor temperature";40004;"°C";s16;10;0;0;0;R;
//
// Limitations:
// - expects double quotes around strings for title, info, unit; no quotes for other fields
// - doesn't handle escaping of quotes
// - could save memory by string de-duplication (unit, empty info)
esp_err_t NibeMqttGwConfigManager::parseNibeModbusCSVLine(const std::string& line, Coil& coil) {
    externbuf buf(line.c_str(), line.size());
    std::istream is(&buf);
    std::string token;
    try {
        // Title
        std::getline(is, token, ';');
        if (!is || token.size() < 2 || !token.starts_with('"') || !token.ends_with('"')) return ESP_FAIL;
        coil.title = token.substr(1, token.size() - 2);
        // Info
        std::getline(is, token, ';');
        if (!is || token.size() < 2 || !token.starts_with('"') || !token.ends_with('"')) return ESP_FAIL;
        coil.info = token.substr(1, token.size() - 2);
        // ID
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.id = std::stoi(token);  // TODO: error handling
        // Unit
        std::getline(is, token, ';');
        if (!is || token.size() < 2 || !token.starts_with('"') || !token.ends_with('"')) return ESP_FAIL;
        coil.unit = token.substr(1, token.size() - 2);
        // Size
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.dataType = nibeModbusSizeToDataType(token);
        if (coil.dataType == COIL_DATA_TYPE_UNKNOWN) return ESP_FAIL;
        // Factor
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.factor = std::stoi(token);
        // Min
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.minValue = std::stoi(token);
        // Max
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.maxValue = std::stoi(token);
        // Default
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.defaultValue = std::stoi(token);
        // Mode
        std::getline(is, token, ';');
        if (!is || token.empty()) return ESP_FAIL;
        coil.mode = nibeModbusMode(token);
        if (coil.mode == COIL_MODE_UNKNOWN) return ESP_FAIL;

        return ESP_OK;
    } catch (const std::invalid_argument& e) {
        // TODO: is not catched on linux but works on esp32, unclear why
        return ESP_FAIL;
    } catch (...) {
        // for linux only to get tests green
        return ESP_FAIL;
    }
}

CoilDataType NibeMqttGwConfigManager::nibeModbusSizeToDataType(const std::string& size) {
    // TODO: more data types like date, needs additional metadata because not encoded in CSV
    if (size == "s8") return COIL_DATA_TYPE_INT8;
    if (size == "s16") return COIL_DATA_TYPE_INT16;
    if (size == "s32") return COIL_DATA_TYPE_INT32;
    if (size == "u8") return COIL_DATA_TYPE_UINT8;
    if (size == "u16") return COIL_DATA_TYPE_UINT16;
    if (size == "u32") return COIL_DATA_TYPE_UINT32;
    return COIL_DATA_TYPE_UNKNOWN;
}

CoilMode NibeMqttGwConfigManager::nibeModbusMode(const std::string& mode) {
    if (mode == "R") return COIL_MODE_READ;
    if (mode == "W") return COIL_MODE_WRITE;
    if (mode == "RW") return COIL_MODE_READ_WRITE;
    return COIL_MODE_UNKNOWN;
}