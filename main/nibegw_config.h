#ifndef _nibegw_config_h_
#define _nibegw_config_h_

#include "config.h"

// ensure that config.h is included before ArduinoJson
#include <ArduinoJson.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "metrics.h"
#include "nibegw.h"

// configuration
enum class NibeRegisterDataType {
    Unknown,
    UInt8,
    Int8,
    UInt16,
    Int16,
    UInt32,
    Int32,
};

enum class NibeRegisterMode {
    Unknown = 0x00,
    Read = 0x01,
    Write = 0x02,
    ReadWrite = Read | Write,
};

enum class NibeRegisterUnit {
    Unknown,
    NoUnit,  // no unit

    GradCelcius,       // °C
    Percent,           // %
    LiterPerMinute,    // l/min
    KiloPascal,        // kPa
    Bar,               // bar
    RelativeHumidity,  // %RH
    RPM,               // rpm

    Volt,          // V
    Ampere,        // A
    Watt,          // W
    KiloWatt,      // kW
    WattHour,      // Wh
    KiloWattHour,  // kWh
    Hertz,         // Hz

    Seconds,  // s, secs
    Minutes,  // min
    Hours,    // h, hrs
    Days,     // days
    Months,   // months
};

struct NibeRegisterMetricConfig;
struct NibeMqttConfig;

// represents register configuration from ModbusManager
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
class NibeRegister {
   public:
    uint16_t id;
    std::string title;
    NibeRegisterUnit unit;
    NibeRegisterDataType dataType;  // = size
    int factor;
    int minValue;
    int maxValue;
    int defaultValue;
    NibeRegisterMode mode;

    NibeRegister() = default;
    NibeRegister(uint16_t id, const std::string& title, NibeRegisterUnit unit, NibeRegisterDataType dataType, int factor, int minValue, int maxValue,
         int defaultValue, NibeRegisterMode mode)
        : id(id),
          title(title),
          unit(unit),
          dataType(dataType),
          factor(factor),
          minValue(minValue),
          maxValue(maxValue),
          defaultValue(defaultValue),
          mode(mode) {}

    int32_t decodeDataRaw(const uint8_t* const data) const;
    std::string decodeData(const uint8_t* const data) const;
    bool encodeData(const std::string& value, uint8_t* data) const;
    std::string formatNumber(auto value) const { return Metrics::formatNumber(value, factor, 1); }
    int32_t parseSignedNumber(const std::string& value) const;
    uint32_t parseUnsignedNumber(const std::string& value) const;
    const char* unitAsString() const;
    static NibeRegisterUnit stringToUnit(const char* unit);

    JsonDocument homeassistantDiscoveryMessage(const NibeMqttConfig& config, const std::string& nibeRootTopic,
                                               const std::string& deviceDiscoveryInfo) const;

    NibeRegisterMetricConfig toPromMetricConfig(const NibeMqttConfig& config) const;
    std::string promMetricName() const;
    void appendPromAttributes(std::string& promMetricName) const;

    bool operator==(const NibeRegister& other) const = default;

   private:
    JsonDocument defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                      const std::string& deviceDiscoveryInfo) const;
};

struct NibeRegisterMetricConfig {
    std::string name;
    int factor;
    int scale;
    bool counter;

    bool isValid() const { return !name.empty() && factor != 0 && scale != 0; }
};

struct NibeMqttConfig {
    std::unordered_map<uint16_t, NibeRegister> registers;  // TODO const NibeRegister, but doesn't work
    std::vector<uint16_t> pollRegisters;
    std::vector<uint16_t> pollRegistersSlow;
    std::unordered_map<uint16_t, NibeRegisterMetricConfig> metrics;
    std::unordered_map<uint16_t, std::string> homeassistantDiscoveryOverrides;
};

#endif