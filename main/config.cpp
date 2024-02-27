#include "config.h"

#include <ArduinoJson.h>
#include <esp_log.h>

#if CONFIG_IDF_TARGET_LINUX
#else
#include <ETH.h>
#include <LittleFS.h>
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
#endif

    // TODO: remove this and parse nibe ModbusManager file instead
    Coil c = {1, "coil1", "info", "unit", COIL_DATA_TYPE_UINT8, 10, 0, 0, 0, COIL_MODE_READ};
    config.nibe.coils[1] = c;
    c = {2, "coil2", "info", "unit", COIL_DATA_TYPE_UINT16, 1, 0, 0, 0, COIL_MODE_READ};
    config.nibe.coils[2] = c;

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