#include "config.h"

#include <ArduinoJson.h>
#include <ETH.h>
#include <LittleFS.h>
#include <esp_log.h>

static const char* TAG = "config";

#define CONFIG_FILE "/config.json"

NibeMqttGwConfigManager::NibeMqttGwConfigManager() {
    // initialize with default values
    config.mqtt = {
        .brokerUri = "mqtt://mosquitto",
        .user = "",
        .password = "",
        .clientId = "",
        .rootTopic = "nibegw",
        .discoveryPrefix = "homeassistant",
        .hostname = ETH.getHostname(),
    };
}

esp_err_t NibeMqttGwConfigManager::begin() {
    if (!LittleFS.begin(true)) {
        ESP_LOGE(TAG, "Failed to mount file system");
        return ESP_FAIL;
    }
    File file = LittleFS.open(CONFIG_FILE);
    if (file) {
        ESP_LOGI(TAG, "Reading config file %s", CONFIG_FILE);

        String json = file.readString();
        file.close();
        NibeMqttGwConfig tmpConfig;
        if (parseJson(json, tmpConfig) != ESP_OK) {
            return ESP_FAIL;
        }
        // use valid config
        config = tmpConfig;
    } else {
        ESP_LOGW(TAG, "Config file %s not found", CONFIG_FILE);
    }

    return ESP_OK;
}

const String NibeMqttGwConfigManager::getConfigAsJson() {
    JsonDocument doc;
    doc["mqtt"]["brokerUri"] = config.mqtt.brokerUri;
    doc["mqtt"]["user"] = config.mqtt.user;
    doc["mqtt"]["password"] = config.mqtt.password;
    doc["mqtt"]["clientId"] = config.mqtt.clientId;
    doc["mqtt"]["rootTopic"] = config.mqtt.rootTopic;
    doc["mqtt"]["discoveryPrefix"] = config.mqtt.discoveryPrefix;
    String json;
    serializeJsonPretty(doc, json);
    return json;
}

esp_err_t NibeMqttGwConfigManager::saveConfig(const String& configJson) {
    // validate config json
    NibeMqttGwConfig tmpConfig;
    if (parseJson(configJson, tmpConfig) != ESP_OK) {
        return ESP_FAIL;
    }

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
    return ESP_OK;
}

static String _defaultClientId() {
    String hostname = ETH.getHostname();
    String clientId = hostname;
    clientId += "-";
    clientId += ETH.macAddress();
    return clientId;
}

esp_err_t NibeMqttGwConfigManager::parseJson(const String& jsonString, NibeMqttGwConfig& config) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        ESP_LOGE(TAG, "deserializeJson() failed: %s", error.c_str());
        return ESP_FAIL;
    }
    config.mqtt.brokerUri = doc["mqtt"]["brokerUri"].as<String>();
    if (config.mqtt.brokerUri.isEmpty()) {
        ESP_LOGE(TAG, "brokerUri is required");
        return ESP_FAIL;
    }
    config.mqtt.user = doc["mqtt"]["user"].as<String>();
    config.mqtt.password = doc["mqtt"]["password"].as<String>();
    config.mqtt.clientId = doc["mqtt"]["clientId"] | _defaultClientId();
    config.mqtt.rootTopic = doc["mqtt"]["rootTopic"] | "nibegw";
    config.mqtt.discoveryPrefix = doc["mqtt"]["discoveryPrefix"] | "homeassistant";
    config.mqtt.hostname = ETH.getHostname();

    return ESP_OK;
}