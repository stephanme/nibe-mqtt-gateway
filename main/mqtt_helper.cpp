#include "mqtt_helper.h"

#include <esp_log.h>

static const char* TAG = "mqtt";

// extra file for testing
// https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901241
// assumes a valid topic filter, e.g. wildcards only for complete topics
bool MqttHelper::matchTopic(const char* topic, const char* filter) {
    while (*topic && *filter) {
        if (*filter == '+') {
            // skip until next '/'
            while (*topic && *topic != '/') {
                topic++;
            }
            filter++;
        } else if (*filter == '#') {
            return true;
        } else if (*topic != *filter) {
            return false;
        } else {
            topic++;
            filter++;
        }
    }
    if (*topic == 0) {
        // single-level wildcard matches exactly one level
        if (*filter == '+') {
            filter++;
        }
        // The multi-level wildcard represents the parent (and must be the last filter char).
        if (*filter == '/' && *(filter + 1) == '#') {
            filter += 2;
        }
    }
    return *topic == *filter;
}

void MqttHelper::mergeMqttDiscoveryInfoOverride(JsonDocument& discoveryDoc, const std::string& override) {
    if (override.empty()) {
        return;
    }
    JsonDocument overrideDoc;
    DeserializationError errOverride = deserializeJson(overrideDoc, override);
    if (errOverride) {
        ESP_LOGE(TAG, "Failed to parse override discovery message (%s): %s", errOverride.c_str(), override.c_str());
        return;
    }
    // merge overrideDoc into discoveryMsgDoc
    for (auto kv : overrideDoc.as<JsonObject>()) {
        if (kv.value().isNull()) {
            discoveryDoc.remove(kv.key());
        } else {
            discoveryDoc[kv.key()] = kv.value();
        }
    }
}
