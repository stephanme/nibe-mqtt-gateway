#ifndef _config_h_
#define _config_h_

#include "mqtt.h"
#include "mqtt_logging.h"
#include "nibegw_config.h"

struct NibeMqttGwConfig {
    MqttConfig mqtt;
    NibeMqttConfig nibe;
    LogConfig logging;
};

// Configuration is stored in SPIFFS as JSON file.
class NibeMqttGwConfigManager {
public:
    NibeMqttGwConfigManager();
    esp_err_t begin();
    const NibeMqttGwConfig& getConfig() { return config; }

    const String getConfigAsJson();
    esp_err_t saveConfig(const String& configJson);

private:
    NibeMqttGwConfig config;

    esp_err_t parseJson(const String& jsonString, NibeMqttGwConfig& config);
};

#endif