#include "metrics.h"

#include <esp_log.h>

static const char* TAG = "metrics";

esp_err_t Metrics::begin() {
    ESP_LOGI(TAG, "begin");
    return ESP_OK;
}

Metric& Metrics::addMetric(const char* name, int factor) {
    // Metric objects are statically pre-allocted
    Metric& m = metrics[numMetrics];
    m.name = name;
    m.factor = factor;
    m.value = 0l;
    if (numMetrics < MAX_METRICS) {
        numMetrics++;
    } else {
        ESP_LOGE(TAG, "max number of metrics reached");
    }
    estimatedSize += m.name.size() + 20;
    return m;
}

Metric* Metrics::findMetric(const char* name) {
    if (name == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < numMetrics; i++) {
        if (metrics[i].name == name) {
            return &metrics[i];
        }
    }
    return nullptr;
}

std::string Metrics::getAllMetricsAsString() {
    std::string s;
    s.reserve(estimatedSize);
    s = "# nibe-mqtt-gateway metrics\n";
    for (int i = 0; i < numMetrics; i++) {
        s += metrics[i].getValueAsString();
        s += "\n";
    }
    return s;
}

std::string Metric::getValueAsString() {
    std::string s;
    s.reserve(name.size() + 20);
    s += name;
    s += " ";
    s += Metrics::formatNumber(value.load(), factor);
    return s;
}