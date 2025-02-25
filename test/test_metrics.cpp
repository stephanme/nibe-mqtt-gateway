#include <unity.h>

#include "metrics.h"


TEST_CASE("counter metrics", "[metrics]") {
    Metric m1("metric1", 1, 1, true);
    m1.setValue(123);
    TEST_ASSERT_EQUAL(123, m1.getValue());
    m1.setValue(122);
    TEST_ASSERT_EQUAL(123, m1.getValue());
    m1.setValue(124);
    TEST_ASSERT_EQUAL(124, m1.getValue());
    m1.incrementValue(1);
    TEST_ASSERT_EQUAL(125, m1.getValue());
    m1.incrementValue(-1);
    TEST_ASSERT_EQUAL(125, m1.getValue());
    m1.incrementValue(0);
    TEST_ASSERT_EQUAL(125, m1.getValue());

    Metric m2("metric2", 1, 1, false);
    m2.setValue(123);
    TEST_ASSERT_EQUAL(123, m2.getValue());
    m2.setValue(122);
    TEST_ASSERT_EQUAL(122, m2.getValue());
    m2.setValue(124);
    TEST_ASSERT_EQUAL(124, m2.getValue());
    m2.incrementValue(1);
    TEST_ASSERT_EQUAL(125, m2.getValue());
    m2.incrementValue(-1);
    TEST_ASSERT_EQUAL(124, m2.getValue());
    m2.incrementValue(0);
    TEST_ASSERT_EQUAL(124, m2.getValue());
}

TEST_CASE("formatNumber", "[metrics]") {
    TEST_ASSERT_EQUAL_STRING("0", Metrics::formatNumber(0, 1, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("10", Metrics::formatNumber(10, 1, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("-1000", Metrics::formatNumber(-1000l, 1, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("255", Metrics::formatNumber((u_int8_t)255, 1, 1).c_str());

    TEST_ASSERT_EQUAL_STRING("0.0", Metrics::formatNumber(0, 10, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("1.0", Metrics::formatNumber(10, 10, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("-100.1", Metrics::formatNumber(-1001l, 10, 1).c_str());

    TEST_ASSERT_EQUAL_STRING("0.00", Metrics::formatNumber(0, 100, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.01", Metrics::formatNumber(1, 100, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.10", Metrics::formatNumber(10, 100, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("1.10", Metrics::formatNumber(110, 100, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("-10.05", Metrics::formatNumber(-1005l, 100, 1).c_str());

    TEST_ASSERT_EQUAL_STRING("0.000", Metrics::formatNumber(0, 1000, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.001", Metrics::formatNumber(1, 1000, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.020", Metrics::formatNumber(20, 1000, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.500", Metrics::formatNumber(500, 1000, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("1.234", Metrics::formatNumber(1234, 1000, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("-1.005", Metrics::formatNumber(-1005l, 1000, 1).c_str());

    TEST_ASSERT_EQUAL_STRING("0.000000", Metrics::formatNumber(0, 2, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.500000", Metrics::formatNumber(1, 2, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("5.000000", Metrics::formatNumber(10, 2, 1).c_str());
    TEST_ASSERT_EQUAL_STRING("-500.500000", Metrics::formatNumber(-1001l, 2, 1).c_str());

    TEST_ASSERT_EQUAL_STRING("0", Metrics::formatNumber(0, 1, 10).c_str());
    TEST_ASSERT_EQUAL_STRING("100", Metrics::formatNumber(10, 1, 10).c_str());
    TEST_ASSERT_EQUAL_STRING("-10000", Metrics::formatNumber(-1000l, 1, 10).c_str());
    TEST_ASSERT_EQUAL_STRING("250", Metrics::formatNumber((u_int8_t)25, 1, 10).c_str());
}

TEST_CASE("add/findMetric", "[metrics]") {
    Metrics m;
    m.begin();
    Metric& metric1 = m.addMetric("metric1", 1);
    TEST_ASSERT_EQUAL_STRING("metric1", metric1.getName().c_str());
    TEST_ASSERT_EQUAL(1, metric1.getFactor());
    TEST_ASSERT_EQUAL(METRIC_UNINITIALIZED, metric1.getValue());
    Metric& metric2 = m.addMetric("metric2", 10);
    TEST_ASSERT_EQUAL_STRING("metric2", metric2.getName().c_str());
    TEST_ASSERT_EQUAL(10, metric2.getFactor());
    TEST_ASSERT_EQUAL(METRIC_UNINITIALIZED, metric2.getValue());

    TEST_ASSERT_EQUAL_PTR(&metric1, m.findMetric("metric1"));
    TEST_ASSERT_EQUAL_PTR(&metric2, m.findMetric("metric2"));
    TEST_ASSERT_NULL(m.findMetric("metric3"));
}

TEST_CASE("getValueAsString", "[metrics]") {
    Metric m1("metric1");
    m1.setValue(123);
    TEST_ASSERT_EQUAL_STRING("metric1 123", m1.getValueAsString().c_str());
    m1.setValue(321);
    TEST_ASSERT_EQUAL_STRING("metric1 321", m1.getValueAsString().c_str());

    Metric m2(R"(metric2{label1="v1",label2="v2")", 10);
    m2.setValue(0);
    m2.incrementValue(123);
    TEST_ASSERT_EQUAL_STRING(R"(metric2{label1="v1",label2="v2" 12.3)", m2.getValueAsString().c_str());

    Metric m3("metric3", 1, 100);
    m3.setValue(123);
    TEST_ASSERT_EQUAL_STRING("metric3 12300", m3.getValueAsString().c_str());
}

TEST_CASE("getAllMetricsAsString", "[metrics]") {
    Metrics m;
    m.begin();
    Metric& m1 = m.addMetric("metric1", 1);
    Metric& m2 = m.addMetric("metric2", 10);

    // metrics are not yet initialized
    TEST_ASSERT_EQUAL_STRING(R"(# nibe-mqtt-gateway metrics
)", m.getAllMetricsAsString().c_str());

    // initialize metrics
    m1.setValue(0);
    m2.setValue(0);

    TEST_ASSERT_EQUAL_STRING(R"(# nibe-mqtt-gateway metrics
metric1 0
metric2 0.0
)", m.getAllMetricsAsString().c_str());
}