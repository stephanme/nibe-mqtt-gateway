#include "nibegw_config.h"

#include <esp_log.h>

#include <cstring>

static const char* TAG = "nibegw_config";

int32_t NibeRegister::decodeDataRaw(const uint8_t* const data) const {
    int32_t value = 0;
    switch (dataType) {
        case NibeRegisterDataType::UInt8:
            value = data[0];
            break;
        case NibeRegisterDataType::Int8:
            value = (int8_t)data[0];
            break;
        case NibeRegisterDataType::UInt16:
            value = *(uint16_t*)data;
            break;
        case NibeRegisterDataType::Int16:
            value = *(int16_t*)data;
            break;
        case NibeRegisterDataType::UInt32:
            value = *(uint32_t*)data;
            break;
        case NibeRegisterDataType::Int32:
            value = *(int32_t*)data;
            break;
        default:
            ESP_LOGW(TAG, "Register %d has unknown data type %d", id, (int)dataType);
            break;
    }
    return value;
}

std::string NibeRegister::decodeData(const uint8_t* const data) const {
    std::string value;
    switch (dataType) {
        case NibeRegisterDataType::UInt8:
            value = formatNumber(data[0]);
            break;
        case NibeRegisterDataType::Int8:
            value = formatNumber((int8_t)data[0]);
            break;
        case NibeRegisterDataType::UInt16:
            value = formatNumber(*(uint16_t*)data);
            break;
        case NibeRegisterDataType::Int16:
            value = formatNumber(*(int16_t*)data);
            break;
        case NibeRegisterDataType::UInt32:
            value = formatNumber(*(uint32_t*)data);
            break;
        case NibeRegisterDataType::Int32:
            value = formatNumber(*(int32_t*)data);
            break;
        default:
            ESP_LOGW(TAG, "Register %d has unknown data type %d", id, (int)dataType);
            break;
    }
    return value;
}

// Returns true if value was successfully encoded, false otherwise.
// TODO: sort out exception problem on linux target
bool NibeRegister::encodeData(const std::string& str, uint8_t* data) const {
    try {
        switch (dataType) {
            case NibeRegisterDataType::UInt8:
            case NibeRegisterDataType::UInt16:
            case NibeRegisterDataType::UInt32: {
                uint32_t numValue = parseUnsignedNumber(str);
                *(uint32_t*)data = numValue;
                break;
            }

            case NibeRegisterDataType::Int8:
            case NibeRegisterDataType::Int16:
            case NibeRegisterDataType::Int32: {
                int32_t numValue = parseSignedNumber(str);
                *(int32_t*)data = numValue;
                break;
            }
            default:
                ESP_LOGW(TAG, "Register %d has unknown data type %d", id, (int)dataType);
                return false;
        }
        return true;

#if CONFIG_IDF_TARGET_LINUX
    } catch (...) {
        // for linux only to get tests green
        // don't use on esp32 because it hides OOMs
        ESP_LOGW(TAG, "Failed to parse number %s for register %d", str.c_str(), id);
        return false;
    }
#else
    } catch (const std::invalid_argument& e) {
        // TODO: is not caught on linux but works on esp32, unclear why
        ESP_LOGW(TAG, "Failed to parse number %s for register %d", str.c_str(), id);
        return false;
    }
#endif
}

// throws std::invalid_argument exception if str can't be parsed
int32_t NibeRegister::parseSignedNumber(const std::string& str) const {
    int32_t value = 0;
    // avoid FP arithmetic on typically used factors
    if (factor == 1) {
        return std::stoi(str);
    } else if (factor == 10 || factor == 100) {
        // split str at decimal point
        size_t pos = str.find(".");
        int numDecimals = factor == 10 ? 1 : 2;
        if (pos != std::string::npos && pos < str.size() - numDecimals) {
            // remove decimal point
            std::string strInt = str.substr(0, pos) + str.substr(pos + 1, numDecimals);
            value = std::stoi(strInt);
        } else {
            value = std::stoi(str) * factor;
        }
    } else {
        float fValue = std::stof(str);
        value = fValue * factor;
    }
    return value;
}

// throws std::invalid_argument exception if str can't be parsed
uint32_t NibeRegister::parseUnsignedNumber(const std::string& str) const {
    uint32_t value = 0;
    // avoid FP arithmetic on typically used factors
    if (factor == 1) {
        return std::stoul(str);
    } else if (factor == 10 || factor == 100) {
        // split str at decimal point
        size_t pos = str.find(".");
        int numDecimals = factor == 10 ? 1 : 2;
        if (pos != std::string::npos && pos < str.size() - numDecimals) {
            // remove decimal point
            std::string strInt = str.substr(0, pos) + str.substr(pos + 1, numDecimals);
            value = std::stoul(strInt);
        } else {
            value = std::stoul(str) * factor;
        }
    } else {
        float fValue = std::stof(str);
        value = fValue * factor;
    }
    return value;
}

NibeRegisterUnit NibeRegister::stringToUnit(const char* unit) {
    if (strcmp(unit, "") == 0) {
        return NibeRegisterUnit::NoUnit;
    } else if (strcmp(unit, "1") == 0) {
        return NibeRegisterUnit::NoUnit;
    } else if (strcmp(unit, " ") == 0) {
        return NibeRegisterUnit::NoUnit;
    } else if (strcmp(unit, "°C") == 0) {  // °C in UTF-8
        return NibeRegisterUnit::GradCelcius;
    } else if (strcmp(unit,
                      "\xB0"
                      "C") == 0) {  // °C in ISO-8859-1
        return NibeRegisterUnit::GradCelcius;
    } else if (strcmp(unit,
                      "\xba"
                      "C") == 0) {  // buggy °C found in csv
        return NibeRegisterUnit::GradCelcius;
    } else if (strcmp(unit, "%") == 0) {
        return NibeRegisterUnit::Percent;
    } else if (strcmp(unit, "l/m") == 0) {
        return NibeRegisterUnit::LiterPerMinute;
    } else if (strcmp(unit, "%RH") == 0) {
        return NibeRegisterUnit::RelativeHumidity;
    } else if (strcmp(unit, "rpm") == 0) {
        return NibeRegisterUnit::RPM;
    } else if (strcmp(unit, "kPa") == 0) {
        return NibeRegisterUnit::KiloPascal;
    } else if (strcmp(unit, "bar") == 0) {
        return NibeRegisterUnit::Bar;
    } else if (strcmp(unit, "V") == 0) {
        return NibeRegisterUnit::Volt;
    } else if (strcmp(unit, "A") == 0) {
        return NibeRegisterUnit::Ampere;
    } else if (strcmp(unit, "W") == 0) {
        return NibeRegisterUnit::Watt;
    } else if (strcmp(unit, "kW") == 0) {
        return NibeRegisterUnit::KiloWatt;
    } else if (strcmp(unit, "Wh") == 0) {
        return NibeRegisterUnit::WattHour;
    } else if (strcmp(unit, "kWh") == 0) {
        return NibeRegisterUnit::KiloWattHour;
    } else if (strcmp(unit, "Hz") == 0) {
        return NibeRegisterUnit::Hertz;
    } else if (strcmp(unit, "s") == 0) {
        return NibeRegisterUnit::Seconds;
    } else if (strcmp(unit, "secs") == 0) {
        return NibeRegisterUnit::Seconds;
    } else if (strcmp(unit, "min") == 0) {
        return NibeRegisterUnit::Minutes;
    } else if (strcmp(unit, "h") == 0) {
        return NibeRegisterUnit::Hours;
    } else if (strcmp(unit, "hrs") == 0) {
        return NibeRegisterUnit::Hours;
    } else if (strcmp(unit, "days") == 0) {
        return NibeRegisterUnit::Days;
    } else if (strcmp(unit, "Months") == 0) {
        return NibeRegisterUnit::Months;
    } else {
        return NibeRegisterUnit::Unknown;
    }
}

const char* NibeRegister::unitAsString() const {
    switch (unit) {
        case NibeRegisterUnit::NoUnit:
            return "";
        case NibeRegisterUnit::GradCelcius:
            return "°C";  // UTF-8
        case NibeRegisterUnit::Percent:
            return "%";
        case NibeRegisterUnit::LiterPerMinute:
            return "l/m";
        case NibeRegisterUnit::KiloPascal:
            return "kPa";
        case NibeRegisterUnit::Bar:
            return "bar";
        case NibeRegisterUnit::RelativeHumidity:
            return "%RH";
        case NibeRegisterUnit::RPM:
            return "rpm";
        case NibeRegisterUnit::Volt:
            return "V";
        case NibeRegisterUnit::Ampere:
            return "A";
        case NibeRegisterUnit::Watt:
            return "W";
        case NibeRegisterUnit::KiloWatt:
            return "kW";
        case NibeRegisterUnit::WattHour:
            return "Wh";
        case NibeRegisterUnit::KiloWattHour:
            return "kWh";
        case NibeRegisterUnit::Hertz:
            return "Hz";
        case NibeRegisterUnit::Seconds:
            return "s";
        case NibeRegisterUnit::Minutes:
            return "min";
        case NibeRegisterUnit::Hours:
            return "h";
        case NibeRegisterUnit::Days:
            return "days";
        case NibeRegisterUnit::Months:
            return "months";
        default:
            return "Unknown";
    }
}

JsonDocument NibeRegister::homeassistantDiscoveryMessage(const NibeMqttConfig& config, const std::string& nibeRootTopic,
                                                 const std::string& deviceDiscoveryInfo) const {
    auto discoveryDoc = defaultHomeassistantDiscoveryMessage(nibeRootTopic, deviceDiscoveryInfo);
    auto iter = config.homeassistantDiscoveryOverrides.find(id);
    if (iter == config.homeassistantDiscoveryOverrides.end()) {
        return discoveryDoc;
    } else {
        auto override = iter->second;
        // parse override json and defDiscoveryMsg and merge them
        JsonDocument overrideDoc;
        DeserializationError errOverride = deserializeJson(overrideDoc, override);
        if (errOverride) {
            ESP_LOGE(TAG, "Failed to parse override discovery message for register %d: %s", id, errOverride.c_str());
            return discoveryDoc;
        }
        // merge overrideDoc into discoveryMsgDoc
        for (auto kv : overrideDoc.as<JsonObject>()) {
            if (kv.value().isNull()) {
                discoveryDoc.remove(kv.key());
            } else {
                discoveryDoc[kv.key()] = kv.value();
            }
        }
        // return merged doc
        return discoveryDoc;
    }
}

JsonDocument NibeRegister::defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                        const std::string& deviceDiscoveryInfo) const {
    JsonDocument discoveryDoc;

    // TODO: ugly, maybe treat discovery info as json everywhere
    char str[deviceDiscoveryInfo.size() + 3];
    str[0] = '{';
    strcpy(str + 1, deviceDiscoveryInfo.c_str());
    str[deviceDiscoveryInfo.size() + 1] = '}';
    str[deviceDiscoveryInfo.size() + 2] = '\0';
    DeserializationError err = deserializeJson(discoveryDoc, str);
    if (err) {
        // should not happen
        ESP_LOGE(TAG, "Failed to parse device discovery info for register %d: %s", id, err.c_str());
    }

    char objId[64];
    snprintf(objId, sizeof(objId), "nibe-%u", id);
    discoveryDoc["obj_id"] = objId;
    discoveryDoc["uniq_id"] = objId;

    discoveryDoc["name"] = title;

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), "%s%u", nibeRootTopic.c_str(), id);
    discoveryDoc["stat_t"] = stateTopic;

    switch (this->unit) {
        case NibeRegisterUnit::Unknown:
        case NibeRegisterUnit::NoUnit:
            break;
        case NibeRegisterUnit::GradCelcius:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "temperature";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case NibeRegisterUnit::Hours:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "duration";
            discoveryDoc["stat_cla"] = "total";
            break;
        case NibeRegisterUnit::Minutes:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "duration";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case NibeRegisterUnit::Watt:
        case NibeRegisterUnit::KiloWatt:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "power";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case NibeRegisterUnit::WattHour:
        case NibeRegisterUnit::KiloWattHour:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "energy";
            discoveryDoc["stat_cla"] = "total";
            break;
        case NibeRegisterUnit::Hertz:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "frequency";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        default:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["stat_cla"] = "measurement";
            break;
    }

    if (mode != NibeRegisterMode::Read) {
        discoveryDoc["_component_"] = "number";
        discoveryDoc.remove("stat_cla");
        char cmdTopic[68];
        snprintf(cmdTopic, sizeof(cmdTopic), "%s/set", stateTopic);
        discoveryDoc["cmd_t"] = cmdTopic;
        discoveryDoc["min"] = minValue;
        discoveryDoc["max"] = maxValue;
        if (factor != 1) {
            discoveryDoc["step"] = 1.0 / factor;
        } else {
            discoveryDoc["step"] = 1;
        }
    } else {
        discoveryDoc["_component_"] = "sensor";
    }

    return discoveryDoc;
}

// prom metric config must be configured explicitly (i.e. register id) but there are defaults for all config values
NibeRegisterMetricConfig NibeRegister::toPromMetricConfig(const NibeMqttConfig& config) const {
    NibeRegisterMetricConfig metricCfg;
    auto metricCfgIter = config.metrics.find(id);
    if (metricCfgIter != config.metrics.end()) {
        metricCfg.name = metricCfgIter->second.name;
        metricCfg.factor = metricCfgIter->second.factor;
        metricCfg.scale = metricCfgIter->second.scale;
        metricCfg.counter = metricCfgIter->second.counter;
        if (metricCfg.name.empty()) {
            metricCfg.name = promMetricName();
        }
        if (metricCfg.factor == 0) {
            metricCfg.factor = factor;
        }
        if (metricCfg.scale == 0) {
            metricCfg.scale = 1;
        }
        appendPromAttributes(metricCfg.name);
    } else {
        metricCfg.factor = 0;
        metricCfg.scale = 0;
        metricCfg.counter = false;
    }
    return metricCfg;
}

// https://prometheus.io/docs/concepts/data_model/
std::string NibeRegister::promMetricName() const {
    std::string name = "nibe";
    if (!title.empty()) {
        name += "_";
        for (char c : title) {
            if (!std::isalnum(c)) {
                c = '_';
            }
            name += c;
        }
    }
    return name;
}

// append register attributes to promMetricName
void NibeRegister::appendPromAttributes(std::string& promMetricName) const {
    size_t attrPos = promMetricName.find("{");
    if (attrPos == std::string::npos) {
        // no attributes yet
        promMetricName += R"({register=")";
        promMetricName += std::to_string(id);
        promMetricName += R"("})";
    } else {
        // append to existing attributes
        std::string registerAttr = R"(register=")";
        registerAttr += std::to_string(id);
        registerAttr += R"(",)";
        promMetricName.insert(attrPos + 1, registerAttr);
    }
}