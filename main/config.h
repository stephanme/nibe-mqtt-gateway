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

typedef std::unordered_map<std::string, const std::string*> string_deduplicate_t;

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
    static const std::string EMPTY_STRING;
    std::string hostname;
    std::string defaultClientId;
    NibeMqttGwConfig config;

    // used to de-duplicate coil info
    string_deduplicate_t coilInfos;

    esp_err_t parseJson(const char* configJson, NibeMqttGwConfig& config);
    static esp_err_t parseNibeModbusCSV(std::istream& is, std::unordered_map<uint16_t, Coil>* coils,
                                        string_deduplicate_t* coilInfoSet);
    static CoilDataType nibeModbusSizeToDataType(const std::string& size);
    static CoilMode nibeModbusMode(const std::string& size);

    // for testing only
   public:
    static esp_err_t parseNibeModbusCSV(const char* csv, std::unordered_map<uint16_t, Coil>* coils,
                                        string_deduplicate_t* coilInfoSet);
    static esp_err_t parseNibeModbusCSVLine(const std::string& line, Coil& coil, string_deduplicate_t* coilInfoSet);
    static esp_err_t getNextCsvToken(std::istream& is, std::string& token);
};

#endif