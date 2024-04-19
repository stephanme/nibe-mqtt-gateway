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
enum class CoilDataType {
    Unknown,
    UInt8,
    Int8,
    UInt16,
    Int16,
    UInt32,
    Int32,
    // Date?
};

enum class CoilMode {
    Unknown = 0x00,
    Read = 0x01,
    Write = 0x02,
    ReadWrite = Read | Write,
};

enum class CoilUnit {
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

struct NibeCoilMetricConfig;
struct NibeMqttConfig;

// represents coil configuration from ModbusManager
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
//
// TODO: make immutable? memory overhead - especially strings?
class Coil {
   public:
    uint16_t id;
    std::string title;
    CoilUnit unit;
    CoilDataType dataType;  // = size
    int factor;
    int minValue;
    int maxValue;
    int defaultValue;
    CoilMode mode;

    Coil() = default;
    Coil(uint16_t id, const std::string& title, CoilUnit unit, CoilDataType dataType, int factor, int minValue, int maxValue,
         int defaultValue, CoilMode mode)
        : id(id),
          title(title),
          unit(unit),
          dataType(dataType),
          factor(factor),
          minValue(minValue),
          maxValue(maxValue),
          defaultValue(defaultValue),
          mode(mode) {}

    int32_t decodeCoilDataRaw(const uint8_t* const data) const;
    std::string decodeCoilData(const uint8_t* const data) const;
    std::string formatNumber(auto value) const { return Metrics::formatNumber(value, factor, 1); }
    const char* unitAsString() const;
    static CoilUnit stringToUnit(const char* unit);

    JsonDocument homeassistantDiscoveryMessage(const NibeMqttConfig& config, const std::string& nibeRootTopic,
                                               const std::string& deviceDiscoveryInfo) const;

    NibeCoilMetricConfig toPromMetricConfig(const NibeMqttConfig& config) const;
    std::string promMetricName() const;
    void appendPromAttributes(std::string& promMetricName) const;

    bool operator==(const Coil& other) const = default;

   private:
    JsonDocument defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                      const std::string& deviceDiscoveryInfo) const;
};

struct NibeCoilMetricConfig {
    std::string name;
    int factor;
    int scale;

    bool isValid() const { return !name.empty() && factor != 0 && scale != 0; }
};

struct NibeMqttConfig {
    std::unordered_map<uint16_t, Coil> coils;  // TODO const Coil, but doesn't work
    std::vector<uint16_t> coilsToPoll;
    std::unordered_map<uint16_t, NibeCoilMetricConfig> metrics;
    std::unordered_map<uint16_t, std::string> homeassistantDiscoveryOverrides;
};

#endif