#include "nibegw_config.h"

#include <esp_log.h>

#include <bit>
#include <cstring>

static const char* TAG = "nibegw_config";

std::string Coil::decodeCoilData(const NibeReadResponseData& data) const {
    std::string value;
    switch (dataType) {
        case CoilDataType::UInt8:
            value = formatNumber(data.value[0]);
            break;
        case CoilDataType::Int8:
            value = formatNumber((int8_t)data.value[0]);
            break;
        case CoilDataType::UInt16:
            value = formatNumber(std::byteswap(*(uint16_t*)data.value));
            break;
        case CoilDataType::Int16:
            value = formatNumber(std::byteswap(*(int16_t*)data.value));
            break;
        case CoilDataType::UInt32:
            value = formatNumber(std::byteswap(*(uint32_t*)data.value));
            break;
        case CoilDataType::Int32:
            value = formatNumber(std::byteswap(*(int32_t*)data.value));
            break;
        default:
            ESP_LOGW(TAG, "Coil %d has unknown data type %d", id, (int)dataType);
            break;
    }
    return value;
}

CoilUnit Coil::stringToUnit(const char* unit) {
    if (strcmp(unit, "") == 0) {
        return CoilUnit::NoUnit;
    } else if (strcmp(unit, "1") == 0) {
        return CoilUnit::NoUnit;
    } else if (strcmp(unit, " ") == 0) {
        return CoilUnit::NoUnit;
    } else if (strcmp(unit, "°C") == 0) {  // °C in UTF-8
        return CoilUnit::GradCelcius;
    } else if (strcmp(unit,
                      "\xB0"
                      "C") == 0) {  // °C in ISO-8859-1
        return CoilUnit::GradCelcius;
    } else if (strcmp(unit,
                      "\xba"
                      "C") == 0) {  // buggy °C found in csv
        return CoilUnit::GradCelcius;
    } else if (strcmp(unit, "%") == 0) {
        return CoilUnit::Percent;
    } else if (strcmp(unit, "l/m") == 0) {
        return CoilUnit::LiterPerMinute;
    } else if (strcmp(unit, "%RH") == 0) {
        return CoilUnit::RelativeHumidity;
    } else if (strcmp(unit, "rpm") == 0) {
        return CoilUnit::RPM;
    } else if (strcmp(unit, "kPa") == 0) {
        return CoilUnit::KiloPascal;
    } else if (strcmp(unit, "bar") == 0) {
        return CoilUnit::Bar;
    } else if (strcmp(unit, "V") == 0) {
        return CoilUnit::Volt;
    } else if (strcmp(unit, "A") == 0) {
        return CoilUnit::Ampere;
    } else if (strcmp(unit, "W") == 0) {
        return CoilUnit::Watt;
    } else if (strcmp(unit, "kW") == 0) {
        return CoilUnit::KiloWatt;
    } else if (strcmp(unit, "Wh") == 0) {
        return CoilUnit::WattHour;
    } else if (strcmp(unit, "kWh") == 0) {
        return CoilUnit::KiloWattHour;
    } else if (strcmp(unit, "Hz") == 0) {
        return CoilUnit::Hertz;
    } else if (strcmp(unit, "s") == 0) {
        return CoilUnit::Seconds;
    } else if (strcmp(unit, "secs") == 0) {
        return CoilUnit::Seconds;
    } else if (strcmp(unit, "min") == 0) {
        return CoilUnit::Minutes;
    } else if (strcmp(unit, "h") == 0) {
        return CoilUnit::Hours;
    } else if (strcmp(unit, "hrs") == 0) {
        return CoilUnit::Hours;
    } else if (strcmp(unit, "days") == 0) {
        return CoilUnit::Days;
    } else if (strcmp(unit, "Months") == 0) {
        return CoilUnit::Months;
    } else {
        return CoilUnit::Unknown;
    }
}

const char* Coil::unitAsString() const {
    switch (unit) {
        case CoilUnit::NoUnit:
            return "";
        case CoilUnit::GradCelcius:
            return "°C"; // UTF-8
        case CoilUnit::Percent:
            return "%";
        case CoilUnit::LiterPerMinute:
            return "l/m";
        case CoilUnit::KiloPascal:
            return "kPa";
        case CoilUnit::Bar:
            return "bar";
        case CoilUnit::RelativeHumidity:
            return "%RH";
        case CoilUnit::RPM:
            return "rpm";
        case CoilUnit::Volt:
            return "V";
        case CoilUnit::Ampere:
            return "A";
        case CoilUnit::Watt:
            return "W";
        case CoilUnit::KiloWatt:
            return "kW";
        case CoilUnit::WattHour:
            return "Wh";
        case CoilUnit::KiloWattHour:
            return "kWh";
        case CoilUnit::Hertz:
            return "Hz";
        case CoilUnit::Seconds:
            return "s";
        case CoilUnit::Minutes:
            return "min";
        case CoilUnit::Hours:
            return "h";
        case CoilUnit::Days:
            return "days";
        case CoilUnit::Months:
            return "months";
        default:
            return "Unknown";
    }
}