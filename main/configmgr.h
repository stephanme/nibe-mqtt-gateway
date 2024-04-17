#ifndef _configmgr_h_
#define _configmgr_h_

#include <esp_log.h>

#include <functional>

#include "config.h"
#include "mqtt.h"
#include "mqtt_logging.h"
#include "nibegw_config.h"
#include "nonstd_stream.h"

struct NibeMqttGwConfig {
    MqttConfig mqtt;
    NibeMqttConfig nibe;
    LogConfig logging;
};

// filter function for coil ids
// typedef bool (* coilFilterFunction_t)(u_int16_t coilId);
typedef std::function<bool(u_int16_t)> coilFilterFunction_t;

// Configuration is stored in SPIFFS as JSON file.
class NibeMqttGwConfigManager {
   public:
    NibeMqttGwConfigManager();
    esp_err_t begin();
    const NibeMqttGwConfig& getConfig() { return config; }

    const std::string getConfigAsJson();
    const std::string getRuntimeConfigAsJson();
    esp_err_t saveConfig(const char* configJson);

    esp_err_t saveNibeModbusConfig(const char* uploadFileName);

    static void setLogLevels(const std::unordered_map<std::string, std::string>& logLevels);
    static esp_log_level_t toLogLevel(const std::string& level);

   private:
    std::string hostname;
    std::string defaultClientId;
    NibeMqttGwConfig config;

    esp_err_t parseJson(const char* configJson, NibeMqttGwConfig& config);
    static CoilDataType nibeModbusSizeToDataType(const std::string& size);
    static CoilMode nibeModbusMode(const std::string& size);

   public:  // for testing only
    static esp_err_t parseNibeModbusCSV(nonstd::istream& is, std::unordered_map<uint16_t, Coil>* coils = nullptr,
                                        coilFilterFunction_t filter = coilFilterAll);
    static esp_err_t parseNibeModbusCSVLine(const std::string& line, Coil& coil);
    static esp_err_t getNextCsvToken(nonstd::istream& is, std::string& token);

    static bool coilFilterAll(u_int16_t id) { return true; }
    bool coilFilterConfigured(u_int16_t id) const;
};

#endif