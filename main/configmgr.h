#ifndef _configmgr_h_
#define _configmgr_h_

#include <esp_log.h>

#include "config.h"
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
    static esp_err_t parseNibeModbusCSV(std::istream& is, std::unordered_map<uint16_t, Coil>* coils);
    static CoilDataType nibeModbusSizeToDataType(const std::string& size);
    static CoilMode nibeModbusMode(const std::string& size);

    // for testing only
   public:
    static esp_err_t parseNibeModbusCSV(const char* csv, std::unordered_map<uint16_t, Coil>* coils);
    static esp_err_t parseNibeModbusCSVLine(const std::string& line, Coil& coil);
    static esp_err_t getNextCsvToken(std::istream& is, std::string& token);
};

#if CONFIG_IDF_TARGET_LINUX
#else
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

#endif