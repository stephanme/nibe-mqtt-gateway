#include "nibegw_config.h"

#include <esp_log.h>

#include <cstring>

static const char* TAG = "nibegw_config";

int32_t Coil::decodeCoilDataRaw(const uint8_t* const data) const {
    int32_t value = 0;
    switch (dataType) {
        case CoilDataType::UInt8:
            value = data[0];
            break;
        case CoilDataType::Int8:
            value = (int8_t)data[0];
            break;
        case CoilDataType::UInt16:
            value = *(uint16_t*)data;
            break;
        case CoilDataType::Int16:
            value = *(int16_t*)data;
            break;
        case CoilDataType::UInt32:
            value = *(uint32_t*)data;
            break;
        case CoilDataType::Int32:
            value = *(int32_t*)data;
            break;
        default:
            ESP_LOGW(TAG, "Coil %d has unknown data type %d", id, (int)dataType);
            break;
    }
    return value;
}

std::string Coil::decodeCoilData(const uint8_t* const data) const {
    std::string value;
    switch (dataType) {
        case CoilDataType::UInt8:
            value = formatNumber(data[0]);
            break;
        case CoilDataType::Int8:
            value = formatNumber((int8_t)data[0]);
            break;
        case CoilDataType::UInt16:
            value = formatNumber(*(uint16_t*)data);
            break;
        case CoilDataType::Int16:
            value = formatNumber(*(int16_t*)data);
            break;
        case CoilDataType::UInt32:
            value = formatNumber(*(uint32_t*)data);
            break;
        case CoilDataType::Int32:
            value = formatNumber(*(int32_t*)data);
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
            return "°C";  // UTF-8
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

JsonDocument Coil::homeassistantDiscoveryMessage(const NibeMqttConfig& config, const std::string& nibeRootTopic,
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
            ESP_LOGE(TAG, "Failed to parse override discovery message for coil %d: %s", id, errOverride.c_str());
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

JsonDocument Coil::defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
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
        ESP_LOGE(TAG, "Failed to parse device discovery info for coil %d: %s", id, err.c_str());
    }

    // TODO: writable coils
    discoveryDoc["_component_"] = "sensor";

    char objId[64];
    snprintf(objId, sizeof(objId), "nibegw-coil-%u", id);
    discoveryDoc["obj_id"] = objId;
    discoveryDoc["uniq_id"] = objId;

    discoveryDoc["name"] = title;

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), "%s%u", nibeRootTopic.c_str(), id);
    discoveryDoc["stat_t"] = stateTopic;

    switch (this->unit) {
        case CoilUnit::Unknown:
        case CoilUnit::NoUnit:
            break;
        case CoilUnit::GradCelcius:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "temperature";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case CoilUnit::Hours:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "duration";
            discoveryDoc["stat_cla"] = "total";
            break;
        case CoilUnit::Minutes:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "duration";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case CoilUnit::Watt:
        case CoilUnit::KiloWatt:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "power";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        case CoilUnit::WattHour:
        case CoilUnit::KiloWattHour:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "energy";
            discoveryDoc["stat_cla"] = "total";
            break;
        case CoilUnit::Hertz:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["dev_cla"] = "frequency";
            discoveryDoc["stat_cla"] = "measurement";
            break;
        default:
            discoveryDoc["unit_of_meas"] = unitAsString();
            discoveryDoc["stat_cla"] = "measurement";
            break;
    }

    return discoveryDoc;
}

// prom metric config must be configured explicitly (i.e. coil id) but there are defaults for all config values
NibeCoilMetricConfig Coil::toPromMetricConfig(const NibeMqttConfig& config) const {
    NibeCoilMetricConfig metricCfg;
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
std::string Coil::promMetricName() const {
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

// append coil attributes to promMetricName
void Coil::appendPromAttributes(std::string& promMetricName) const {
    size_t attrPos = promMetricName.find("{");
    if (attrPos == std::string::npos) {
        // no attributes yet
        promMetricName += R"({coil=")";
        promMetricName += std::to_string(id);
        promMetricName += R"("})";
    } else {
        // append to existing attributes
        std::string coilAttr = R"(coil=")";
        coilAttr += std::to_string(id);
        coilAttr += R"(",)";
        promMetricName.insert(attrPos + 1, coilAttr);
    }
}