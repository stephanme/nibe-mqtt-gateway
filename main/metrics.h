#ifndef _metrics_h_
#define _metrics_h_

#include <esp_err.h>

#include <atomic>
#include <climits>
#include <string>

#define MAX_METRICS 128

// indicates a metric w/o a value
// uninitialized metrics are not included in getAllMetricsAsString() to avoid e.g. broken counter metrics
#define METRIC_UNINITIALIZED INT_MAX

// Prometheus like metric
// Features/Limitations:
// - only static attributes included in name string
// - raw metric is int32_t, no floating point
// - metrics formatted as float using a scaling factors
// - thread safe: atomic read/increment/set of value, name and factor are immutable
class Metric {
   public:
    Metric() {}
    Metric(const char* name, int factor = 1, int scale = 1) : name(name), factor(factor), scale(scale) {}

    const std::string& getName() const { return name; }
    int getFactor() const { return factor; }

    void setValue(int32_t value) { this->value = value; }
    int32_t incrementValue(int32_t increment) { return value += increment; }
    int32_t getValue() const { return value; }
    bool isInitialized() const { return value != METRIC_UNINITIALIZED; }

    std::string getValueAsString();

   private:
    std::string name;
    // value is formatted as value * scale / factor
    int factor;  // same semantic as in nibe modbus csv
    int scale;
    std::atomic<int32_t> value = METRIC_UNINITIALIZED;

    friend class Metrics;
};

// Prometheus like metric store
// - adding and getting metrics is thread safe
// - getAllMetricsAsString() reports latest values (no consistency)
class Metrics {
   public:
    Metrics() {}

    esp_err_t begin();

    Metric& addMetric(const char* name, int factor = 1, int scale = 1);
    Metric* findMetric(const char* name);

    std::string getAllMetricsAsString();

    // avoid FP arithmetic
    static std::string formatNumber(auto value, int factor, int scale) {
        value *= scale;
        if (factor == 1) {
            return std::to_string(value);
        } else if (factor == 10) {
            auto s = std::to_string(value / 10);
            s += ".";
            s += std::to_string(abs(value) % 10);
            return s;
        } else if (factor == 100) {
            auto s = std::to_string(value / 100);
            auto remainder = abs(value % 100);
            if (remainder < 10) {
                s += ".0";
            } else {
                s += ".";
            }
            s += std::to_string(remainder);
            return s;
        } else if (factor == 1000) {
            auto s = std::to_string(value / 1000);
            auto remainder = abs(value % 1000);
            if (remainder < 10) {
                s += ".00";
            } else if (remainder < 100) {
                s += ".0";
            } else {
                s += ".";
            }
            s += std::to_string(remainder);
            return s;
        } else {
            char s[30];
            snprintf(s, sizeof(s), "%f", (float)value / factor);
            return s;
        }
    }
    // std::abs is not defined for unsigned types but needed by formatNumber()
    template <typename T>
    static T abs(T value) {
        return value >= 0 ? value : -value;
    }

   private:
    Metric metrics[MAX_METRICS];  // TODO: vector or simple pre-allocated array?
    std::atomic<int> numMetrics = 0;
    std::atomic<int> estimatedSize = 0;
};

#endif