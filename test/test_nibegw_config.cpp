#include <esp_log.h>
#include <unity.h>

#include <exception>
#include <stdexcept>

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

void assertCoilData(const uint8_t actual[4], uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    TEST_ASSERT_EQUAL_UINT8(byte1, actual[0]);
    TEST_ASSERT_EQUAL_UINT8(byte2, actual[1]);
    TEST_ASSERT_EQUAL_UINT8(byte3, actual[2]);
    TEST_ASSERT_EQUAL_UINT8(byte4, actual[3]);
}

TEST_CASE("encodeCoilData", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    uint8_t data[4] = {0, 0, 0, 0};

    TEST_ASSERT_TRUE(c.encodeCoilData("1", data));
    assertCoilData(data, 1, 0, 0, 0);
    TEST_ASSERT_TRUE(c.encodeCoilData("255", data));
    assertCoilData(data, 0xff, 0, 0, 0);

    c.dataType = CoilDataType::Int8;
    TEST_ASSERT_TRUE(c.encodeCoilData("1", data));
    assertCoilData(data, 1, 0, 0, 0);
    TEST_ASSERT_TRUE(c.encodeCoilData("-1", data));
    assertCoilData(data, 0xff, 0xff, 0xff, 0xff);

    c.dataType = CoilDataType::UInt16;
    TEST_ASSERT_TRUE(c.encodeCoilData("513", data));
    assertCoilData(data, 1, 2, 0, 0);
    TEST_ASSERT_TRUE(c.encodeCoilData("65535", data));
    assertCoilData(data, 0xff, 0xff, 0, 0);

    c.dataType = CoilDataType::Int16;
    TEST_ASSERT_TRUE(c.encodeCoilData("513", data));
    assertCoilData(data, 1, 2, 0, 0);
    TEST_ASSERT_TRUE(c.encodeCoilData("-1", data));
    assertCoilData(data, 0xff, 0xff, 0xff, 0xff);

    c.dataType = CoilDataType::UInt32;
    TEST_ASSERT_TRUE(c.encodeCoilData("67305985", data));
    assertCoilData(data, 1, 2, 3, 4);
    TEST_ASSERT_TRUE(c.encodeCoilData("4294967295", data));
    assertCoilData(data, 0xff, 0xff, 0xff, 0xff);

    c.dataType = CoilDataType::Int32;
    TEST_ASSERT_TRUE(c.encodeCoilData("67305985", data));
    assertCoilData(data, 1, 2, 3, 4);
    TEST_ASSERT_TRUE(c.encodeCoilData("-1", data));
    assertCoilData(data, 0xff, 0xff, 0xff, 0xff);

    // bad data
    TEST_ASSERT_FALSE(c.encodeCoilData("", data));
    TEST_ASSERT_FALSE(c.encodeCoilData("x", data));

    // not yet implemented types
    c.dataType = CoilDataType::Unknown;
    TEST_ASSERT_FALSE(c.encodeCoilData("1", data));
}

void testParseSignedNumberException(const Coil& c, const char* number) {
    try {
        c.parseSignedNumber(number);
        TEST_FAIL_MESSAGE("Expected std::invalid_argument exception");
    } catch (std::invalid_argument& e) {
        // expected
    } catch (...) {
        // linux target: throws std::invalid_argument but it is not caught by the catch block above
        // libc++abi: terminating due to uncaught exception of type std::invalid_argument: stoi: no conversion
    }
}

TEST_CASE("parseSignedNumber", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};

    TEST_ASSERT_EQUAL(0, c.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("1.1"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("10"));
    TEST_ASSERT_EQUAL(-1000, c.parseSignedNumber("-1000"));
    // bad number format
    testParseSignedNumberException(c, "x");
    testParseSignedNumberException(c, "");

    c.factor = 10;
    TEST_ASSERT_EQUAL(0, c.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("0.1"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("1.0"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("1.00"));
    TEST_ASSERT_EQUAL(-1001, c.parseSignedNumber("-100.1"));
    TEST_ASSERT_EQUAL(-1001, c.parseSignedNumber("-100.123"));
    // bad number format
    testParseSignedNumberException(c, "x");
    testParseSignedNumberException(c, "");

    c.factor = 100;
    TEST_ASSERT_EQUAL(0, c.parseSignedNumber("0.00"));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("0.01"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("0.10"));
    TEST_ASSERT_EQUAL(100, c.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(100, c.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(100, c.parseSignedNumber("1.0"));
    TEST_ASSERT_EQUAL(100, c.parseSignedNumber("1.00"));
    TEST_ASSERT_EQUAL(100, c.parseSignedNumber("1.000"));
    TEST_ASSERT_EQUAL(-1005, c.parseSignedNumber("-10.05"));
    TEST_ASSERT_EQUAL(-1005, c.parseSignedNumber("-10.0599"));
    // bad number format
    testParseSignedNumberException(c, "x");
    testParseSignedNumberException(c, "");

    c.factor = 2;
    TEST_ASSERT_EQUAL(0, c.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseSignedNumber("0.5"));
    TEST_ASSERT_EQUAL(10, c.parseSignedNumber("5.0"));
    TEST_ASSERT_EQUAL(-1001, c.parseSignedNumber("-500.5000"));
    // bad number format
    testParseSignedNumberException(c, "x");
    testParseSignedNumberException(c, "");
}

void testParseUnsignedNumberException(const Coil& c, const char* number) {
    try {
        c.parseUnsignedNumber(number);
        TEST_FAIL_MESSAGE("Expected std::invalid_argument exception");
    } catch (std::invalid_argument& e) {
        // expected
    } catch (...) {
        // linux target: throws std::invalid_argument but it is not caught by the catch block above
        // libc++abi: terminating due to uncaught exception of type std::invalid_argument: stoi: no conversion
    }
}

TEST_CASE("parseUnsignedNumber", "[nibegw_config]") {
    Coil c = {0, "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};

    TEST_ASSERT_EQUAL(0, c.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("1.1"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("10"));
    TEST_ASSERT_EQUAL(4294967295, c.parseUnsignedNumber("4294967295"));
    TEST_ASSERT_EQUAL(4294967295, c.parseUnsignedNumber("-1"));  // not exactly nice but that's how std::stoul works
    // bad number format
    testParseUnsignedNumberException(c, "x");
    testParseUnsignedNumberException(c, "");

    c.factor = 10;
    TEST_ASSERT_EQUAL(0, c.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("0.1"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("1.0"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("1.00"));
    TEST_ASSERT_EQUAL(4294967290, c.parseUnsignedNumber("429496729"));
    // bad number format
    testParseUnsignedNumberException(c, "x");
    testParseUnsignedNumberException(c, "");

    c.factor = 100;
    TEST_ASSERT_EQUAL(0, c.parseUnsignedNumber("0.00"));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("0.01"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("0.10"));
    TEST_ASSERT_EQUAL(100, c.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(100, c.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(100, c.parseUnsignedNumber("1.0"));
    TEST_ASSERT_EQUAL(100, c.parseUnsignedNumber("1.00"));
    TEST_ASSERT_EQUAL(100, c.parseUnsignedNumber("1.000"));
    TEST_ASSERT_EQUAL(4294967200, c.parseUnsignedNumber("42949672"));
    // bad number format
    testParseUnsignedNumberException(c, "x");
    testParseUnsignedNumberException(c, "");

    c.factor = 2;
    TEST_ASSERT_EQUAL(0, c.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, c.parseUnsignedNumber("0.5"));
    TEST_ASSERT_EQUAL(10, c.parseUnsignedNumber("5.0"));
    TEST_ASSERT_EQUAL(4294967295, c.parseUnsignedNumber("2147483647"));  // precision problem
    // bad number format
    testParseUnsignedNumberException(c, "x");
    testParseUnsignedNumberException(c, "");
}

TEST_CASE("homeassistantDiscoveryMessage Temperature", "[nibegw_config]") {
    NibeMqttConfig config;
    Coil c = {1, "Temperature", CoilUnit::GradCelcius, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("dev":{"name":"Nibe GW"})";
    auto doc = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("sensor", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Temperature", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["stat_t"]);
    TEST_ASSERT_EQUAL_STRING("°C", doc["unit_of_meas"]);
    TEST_ASSERT_EQUAL_STRING("temperature", doc["dev_cla"]);
    TEST_ASSERT_EQUAL_STRING("measurement", doc["stat_cla"]);
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["dev"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage NoUnit", "[nibegw_config]") {
    NibeMqttConfig config;
    Coil c = {1, "No Unit", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("sensor", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("No Unit", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["stat_t"]);
    TEST_ASSERT_TRUE(doc["unit_of_meas"].isUnbound());
    TEST_ASSERT_TRUE(doc["stat_cla"].isUnbound());
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage Read/write", "[nibegw_config]") {
    NibeMqttConfig config;
    Coil c = {1, "Temperature", CoilUnit::GradCelcius, CoilDataType::UInt8, 10, 0, 100, 0, CoilMode::ReadWrite};
    std::string deviceDiscoveryInfo = R"("dev":{"name":"Nibe GW"})";
    auto doc = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("number", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Temperature", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["stat_t"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1/set", doc["cmd_t"]);
    TEST_ASSERT_EQUAL_STRING("°C", doc["unit_of_meas"]);
    TEST_ASSERT_EQUAL_STRING("temperature", doc["dev_cla"]);
    TEST_ASSERT_EQUAL(0, doc["min"]);
    TEST_ASSERT_EQUAL(100, doc["max"]);
    TEST_ASSERT_TRUE(0.1 == doc["step"]);
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["dev"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage Override", "[nibegw_config]") {
    NibeMqttConfig config;
    config.homeassistantDiscoveryOverrides[1] =
        R"({"_component_":"mysensor","unit_of_meas":"Grad Celsius", "dev_cla":null, "added":123, "removeNonexistingKey":null})";
    Coil c = {1, "Override", CoilUnit::GradCelcius, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("mysensor", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-1", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Override", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/1", doc["stat_t"]);
    TEST_ASSERT_EQUAL_STRING("Grad Celsius", doc["unit_of_meas"]);  // changed by override
    TEST_ASSERT_TRUE(doc["dev_cla"].isUnbound());                   // removed by override
    TEST_ASSERT_EQUAL(123, doc["added"]);                           // added by override, integer type
    TEST_ASSERT_TRUE(doc["removeNonexistingKey"].isUnbound());      // ignore removing of nonexisting key
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("homeassistantDiscoveryMessage Degree Minutes", "[nibegw_config]") {
    NibeMqttConfig config;
    config.homeassistantDiscoveryOverrides[43005] = R"({"stat_cla":"measurement"})";
    Coil c = {43005, "Degree Minutes", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = c.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("sensor", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Degree Minutes", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/43005", doc["stat_t"]);
    TEST_ASSERT_TRUE(doc["unit_of_meas"].isUnbound());
    TEST_ASSERT_EQUAL_STRING("measurement", doc["stat_cla"]);
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