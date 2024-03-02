#ifndef _nibegw_config_h_
#define _nibegw_config_h_

#include <string>
#include <unordered_map>
#include <vector>

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
    Coil(uint16_t id, const std::string& title, CoilUnit unit, CoilDataType dataType, int factor,
         int minValue, int maxValue, int defaultValue, CoilMode mode)
        : id(id),
          title(title),
          unit(unit),
          dataType(dataType),
          factor(factor),
          minValue(minValue),
          maxValue(maxValue),
          defaultValue(defaultValue),
          mode(mode) {}

    std::string decodeCoilData(const NibeReadResponseData& data) const;
    std::string formatNumber(auto value) const;
    const char* unitAsString() const;
    static CoilUnit stringToUnit(const char* unit);

    bool operator==(const Coil& other) const = default;
};

struct NibeMqttConfig {
    std::unordered_map<uint16_t, Coil> coils;  // TODO const Coil, but doesn't work
    std::vector<uint16_t> coilsToPoll;
};

// avoid FP arithmetic
std::string Coil::formatNumber(auto value) const {
    if (factor == 1) {
        return std::to_string(value);
    } else if (factor == 10) {
        auto s = std::to_string(value / 10);
        s += ".";
        s += std::to_string((value >= 0 ? value : -value) % 10);
        return s;
    } else if (factor == 100) {
        auto s = std::to_string(value / 100);
        if (value % 100 < 10) {
            s += ".0";
        } else {
            s += ".";
        }
        s += std::to_string((value >= 0 ? value : -value) % 100);
        return s;
    } else {
        char s[30];
        snprintf(s, sizeof(s), "%f", (float)value / factor);
        return s;
    }
}

#endif