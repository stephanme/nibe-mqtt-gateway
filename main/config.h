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

    const std::string getConfigAsJson();
    esp_err_t saveConfig(const char* configJson);

   private:
    std::string hostname;
    std::string defaultClientId;
    NibeMqttGwConfig config;

    esp_err_t parseJson(const char* configJson, NibeMqttGwConfig& config);
};

#endif