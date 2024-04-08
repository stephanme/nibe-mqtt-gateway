#include "nibegw_config.h"

#include <ArduinoJson.h>
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

std::string Coil::homeassistantDiscoveryMessage(const NibeMqttConfig& config, const std::string& nibeRootTopic,
                                                const std::string& deviceDiscoveryInfo) const {
    auto defDiscoveryMsg = defaultHomeassistantDiscoveryMessage(nibeRootTopic, deviceDiscoveryInfo);
    auto iter = config.homeassistantDiscoveryOverrides.find(id);
    if (iter == config.homeassistantDiscoveryOverrides.end()) {
        return defDiscoveryMsg;
    } else {
        auto override = iter->second;
        // parse override json and defDiscoveryMsg and merge them
        JsonDocument discoveryMsgDoc;
        DeserializationError errDefMsg = deserializeJson(discoveryMsgDoc, defDiscoveryMsg);
        JsonDocument overrideDoc;
        DeserializationError errOverride = deserializeJson(overrideDoc, override);
        if (errDefMsg) {
            ESP_LOGE(TAG, "Failed to parse default discovery message for coil %d: %s", id, errDefMsg.c_str());
            return defDiscoveryMsg;
        }
        if (errOverride) {
            ESP_LOGE(TAG, "Failed to parse override discovery message for coil %d: %s", id, errOverride.c_str());
            return defDiscoveryMsg;
        }
        // merge overrideDoc into discoveryMsgDoc
        for (auto kv : overrideDoc.as<JsonObject>()) {
            if (kv.value().isNull()) {
                discoveryMsgDoc.remove(kv.key());
            } else {
                discoveryMsgDoc[kv.key()] = kv.value();
            }
        }
        // return serialized merged doc
        std::string mergedDiscoveryMsg;
        serializeJson(discoveryMsgDoc, mergedDiscoveryMsg);
        return mergedDiscoveryMsg;
    }
}

static const char* DISCOVERY_PAYLOAD = R"({
"object_id":"nibegw-coil-%u",
"unique_id":"nibegw-coil-%u",
"name":"%s",
"state_topic":"%s",
%s
%s
})";

std::string Coil::defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                       const std::string& deviceDiscoveryInfo) const {
    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), "%s%u", nibeRootTopic.c_str(), id);
    char unit[128];  // TODO: rename
    switch (this->unit) {
        case CoilUnit::Unknown:
        case CoilUnit::NoUnit:
            unit[0] = '\0';
            break;
        case CoilUnit::GradCelcius:
            snprintf(unit, sizeof(unit),
                     R"("unit_of_measurement":"%s","device_class":"temperature","state_class":"measurement",)", unitAsString());
            break;
        case CoilUnit::Hours:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"duration","state_class":"total",)",
                     unitAsString());
            break;
        case CoilUnit::Minutes:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"duration","state_class":"measurement",)",
                     unitAsString());
            break;
        case CoilUnit::Watt:
        case CoilUnit::KiloWatt:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"power","state_class":"measurement",)",
                     unitAsString());
            break;
        case CoilUnit::WattHour:
        case CoilUnit::KiloWattHour:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"energy","state_class":"total",)",
                     unitAsString());
            break;
        case CoilUnit::Hertz:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"frequency","state_class":"measurement",)",
                     unitAsString());
            break;
        default:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","state_class":"measurement",)", unitAsString());
            break;
    }

    // TODO: writable coils
    char discoveryPayload[1024];
    snprintf(discoveryPayload, sizeof(discoveryPayload), DISCOVERY_PAYLOAD, id, id, title.c_str(), stateTopic, unit,
             deviceDiscoveryInfo.c_str());

    return discoveryPayload;
}

// prom metric config must be configured explicitly (i.e. coil id) but there are defaults for all config values
NibeCoilMetricConfig Coil::toPromMetricConfig(const NibeMqttConfig& config) const {
    NibeCoilMetricConfig metricCfg;
    auto metricCfgIter = config.metrics.find(id);
    if (metricCfgIter != config.metrics.end()) {
        metricCfg.name = metricCfgIter->second.name;
        metricCfg.factor = metricCfgIter->second.factor;
        metricCfg.scale = metricCfgIter->second.scale;
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