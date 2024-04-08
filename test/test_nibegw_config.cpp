#include <ArduinoJson.h>
#include <unity.h>

#include "nibegw_config.h"

TEST_CASE("formatNumber", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    TEST_ASSERT_EQUAL_STRING("0", c.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("10", c.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-1000", c.formatNumber(-1000l).c_str());
    TEST_ASSERT_EQUAL_STRING("255", c.formatNumber((u_int8_t)255).c_str());

    c.factor = 10;
    TEST_ASSERT_EQUAL_STRING("0.0", c.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("1.0", c.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-100.1", c.formatNumber(-1001l).c_str());

    c.factor = 100;
    TEST_ASSERT_EQUAL_STRING("0.00", c.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.01", c.formatNumber(1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.10", c.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("1.10", c.formatNumber(110).c_str());
    TEST_ASSERT_EQUAL_STRING("-10.05", c.formatNumber(-1005l).c_str());

    c.factor = 2;
    TEST_ASSERT_EQUAL_STRING("0.000000", c.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.500000", c.formatNumber(1).c_str());
    TEST_ASSERT_EQUAL_STRING("5.000000", c.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-500.500000", c.formatNumber(-1001l).c_str());
}

TEST_CASE("decodeCoilData", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    uint8_t data1234[] = {1, 2, 3, 4};
    uint8_t dataFF[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_STRING("1", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("255", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int8;
    TEST_ASSERT_EQUAL_STRING("1", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::UInt16;
    TEST_ASSERT_EQUAL_STRING("513", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("65535", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int16;
    TEST_ASSERT_EQUAL_STRING("513", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::UInt32;
    TEST_ASSERT_EQUAL_STRING("67305985", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("4294967295", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int32;
    TEST_ASSERT_EQUAL_STRING("67305985", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    // not yet implemented types
    c.dataType = CoilDataType::Unknown;
    TEST_ASSERT_EQUAL_STRING("", c.decodeCoilData(data1234).c_str());
}

TEST_CASE("homeassistantDiscoveryMessage Temperature", "[nibegw_config]") {
    NibeMqttConfig config;
    Coil c = {1, "Temperature", CoilUnit::GradCelcius, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto discoveryMsg = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    // must be valid json
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, discoveryMsg);
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["object_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["unique_id"]);
    TEST_ASSERT_EQUAL_STRING("Temperature", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["state_topic"]);
    TEST_ASSERT_EQUAL_STRING("°C", doc["unit_of_measurement"]);
    TEST_ASSERT_EQUAL_STRING("temperature", doc["device_class"]);
    TEST_ASSERT_EQUAL_STRING("measurement", doc["state_class"]);
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage NoUnit", "[nibegw_config]") {
    NibeMqttConfig config;
    Coil c = {1, "No Unit", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto discoveryMsg = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    // must be valid json
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, discoveryMsg);
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["object_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["unique_id"]);
    TEST_ASSERT_EQUAL_STRING("No Unit", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["state_topic"]);
    TEST_ASSERT_TRUE(doc["unit_of_measurement"].isUnbound());
    TEST_ASSERT_TRUE(doc["state_class"].isUnbound());
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage Override", "[nibegw_config]") {
    NibeMqttConfig config;
    config.homeassistantDiscoveryOverrides[1] =
        R"({"unit_of_measurement":"Grad Celsius", "device_class":null, "added":123, "removeNonexistingKey":null})";
    Coil c = {1, "Override", CoilUnit::GradCelcius, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto discoveryMsg = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    // must be valid json
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, discoveryMsg);
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["object_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["unique_id"]);
    TEST_ASSERT_EQUAL_STRING("Override", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["state_topic"]);
    TEST_ASSERT_EQUAL_STRING("Grad Celsius", doc["unit_of_measurement"]);  // changed by override
    TEST_ASSERT_TRUE(doc["device_class"].isUnbound());                     // removed by override
    TEST_ASSERT_EQUAL(123, doc["added"]);                                  // added by override, integer type
    TEST_ASSERT_TRUE(doc["removeNonexistingKey"].isUnbound());             // ignore removing of nonexisting key
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage Degree Minutes", "[nibegw_config]") {
    NibeMqttConfig config;
    config.homeassistantDiscoveryOverrides[43005] = R"({"state_class":"measurement"})";
    Coil c = {43005, "Degree Minutes", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto discoveryMsg = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    // must be valid json
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, discoveryMsg);
    TEST_ASSERT_FALSE(error);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["object_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["unique_id"]);
    TEST_ASSERT_EQUAL_STRING("Degree Minutes", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/43005", doc["state_topic"]);
    TEST_ASSERT_TRUE(doc["unit_of_measurement"].isUnbound());
    TEST_ASSERT_EQUAL_STRING("measurement", doc["state_class"]);
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("decodeCoilDataRaw", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    uint8_t data1234[] = {1, 2, 3, 4};
    uint8_t dataFF[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_UINT8(1, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT8(255, c.decodeCoilDataRaw(dataFF));

    c.dataType = CoilDataType::Int8;
    TEST_ASSERT_EQUAL_INT8(1, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT8(-1, c.decodeCoilDataRaw(dataFF));

    c.dataType = CoilDataType::UInt16;
    TEST_ASSERT_EQUAL_UINT16(513, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT16(65535, c.decodeCoilDataRaw(dataFF));

    c.dataType = CoilDataType::Int16;
    TEST_ASSERT_EQUAL_INT16(513, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT16(-1, c.decodeCoilDataRaw(dataFF));

    c.dataType = CoilDataType::UInt32;
    TEST_ASSERT_EQUAL_UINT32(67305985, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT32(4294967295, c.decodeCoilDataRaw(dataFF));

    c.dataType = CoilDataType::Int32;
    TEST_ASSERT_EQUAL_INT32(67305985, c.decodeCoilDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT32(-1, c.decodeCoilDataRaw(dataFF));

    // not yet implemented types
    c.dataType = CoilDataType::Unknown;
    TEST_ASSERT_EQUAL(0, c.decodeCoilDataRaw(data1234));
}

TEST_CASE("promMetricName", "[nibegw_config]") {
    Coil c = {1, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    TEST_ASSERT_EQUAL_STRING(R"(nibe)", c.promMetricName().c_str());
    c.title = "Test123";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_Test123)", c.promMetricName().c_str());
    c.title = "Test 123";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_Test_123)", c.promMetricName().c_str());
    c.title = "1Test";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_1Test)", c.promMetricName().c_str());
}

TEST_CASE("appendPromAttributes", "[nibegw_config]") {
    Coil c = {1, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string s = "test";
    c.appendPromAttributes(s);
    TEST_ASSERT_EQUAL_STRING(R"(test{coil="1"})", s.c_str());

    s = R"(test{attr="value"})";
    c.appendPromAttributes(s);
    TEST_ASSERT_EQUAL_STRING(R"(test{coil="1",attr="value"})", s.c_str());
}

TEST_CASE("toPromMetricConfig", "[nibegw_config]") {
    Coil c = {1, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    NibeMqttConfig config;
    config.metrics[1] = {"test123", 10, 1};
    config.metrics[2] = {"", 10, 10};
    config.metrics[3] = {R"(test123{node="nibegw"})", 0, 0};

    NibeCoilMetricConfig metricCfg = c.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(test123{coil="1"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(10, metricCfg.factor);
    TEST_ASSERT_EQUAL(1, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    c.id = 2;
    metricCfg = c.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(nibe{coil="2"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(10, metricCfg.factor);
    TEST_ASSERT_EQUAL(10, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    c.id = 3;
    metricCfg = c.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(test123{coil="3",node="nibegw"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(1, metricCfg.factor);
    TEST_ASSERT_EQUAL(1, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    // metric not in config -> do not create a valid metric
    c.id = 4;
    metricCfg = c.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING("", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(0, metricCfg.factor);
    TEST_ASSERT_EQUAL(0, metricCfg.scale);
    TEST_ASSERT_FALSE(metricCfg.isValid());
}