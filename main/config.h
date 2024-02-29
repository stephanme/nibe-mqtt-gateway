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

    const std::string getNibeModbusConfig();
    esp_err_t saveNibeModbusConfig(const char* csv);

   private:
    std::string hostname;
    std::string defaultClientId;
    NibeMqttGwConfig config;

    esp_err_t parseJson(const char* configJson, NibeMqttGwConfig& config);
    static esp_err_t parseNibeModbusCSV(std::istream& is, std::unordered_map<uint16_t, Coil>& coils);
    static CoilDataType nibeModbusSizeToDataType(const std::string& size);
    static CoilMode nibeModbusMode(const std::string& size);

    // for testing only
   public:
    static esp_err_t parseNibeModbusCSV(const char* csv, std::unordered_map<uint16_t, Coil>& coils);
    static esp_err_t parseNibeModbusCSVLine(const std::string& line, Coil& coil);
};

#endif