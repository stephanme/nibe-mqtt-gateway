#ifndef _nibegw_config_h_
#define _nibegw_config_h_

#include <string>
#include <unordered_map>
#include <vector>

#include "nibegw.h"

// configuration
enum CoilDataType {
    COIL_DATA_TYPE_UINT8,
    COIL_DATA_TYPE_INT8,
    COIL_DATA_TYPE_UINT16,
    COIL_DATA_TYPE_INT16,
    COIL_DATA_TYPE_UINT32,
    COIL_DATA_TYPE_INT32,
    COIL_DATA_TYPE_DATE,
};

enum CoilMode {
    COIL_MODE_READ = 0x01,
    COIL_MODE_WRITE = 0x02,
    COIL_MODE_READ_WRITE = COIL_MODE_READ | COIL_MODE_WRITE,
};

// represents coil configuration from ModbusManager
// Title;Info;ID;Unit;Size;Factor;Min;Max;Default;Mode
class Coil {
   public:
    uint16_t id;
    const char* title;
    const char* info;
    const char* unit;
    CoilDataType dataType;  // = size
    int factor;
    int minValue;
    int maxValue;
    int defaultValue;
    CoilMode mode;

    std::string decodeCoilData(const NibeReadResponseData& data) const;
    std::string formatNumber(auto value) const;
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