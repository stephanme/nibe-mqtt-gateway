#include <esp_log.h>
#include <unity.h>

#include <exception>
#include <stdexcept>

#include "nibegw_config.h"

TEST_CASE("formatNumber", "[nibegw_config]") {
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    TEST_ASSERT_EQUAL_STRING("0", r.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("10", r.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-1000", r.formatNumber(-1000l).c_str());
    TEST_ASSERT_EQUAL_STRING("255", r.formatNumber((u_int8_t)255).c_str());

    r.factor = 10;
    TEST_ASSERT_EQUAL_STRING("0.0", r.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("1.0", r.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-100.1", r.formatNumber(-1001l).c_str());

    r.factor = 100;
    TEST_ASSERT_EQUAL_STRING("0.00", r.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.01", r.formatNumber(1).c_str());
    TEST_ASSERT_EQUAL_STRING("0.10", r.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("1.10", r.formatNumber(110).c_str());
    TEST_ASSERT_EQUAL_STRING("-10.05", r.formatNumber(-1005l).c_str());

    r.factor = 2;
    TEST_ASSERT_EQUAL_STRING("0.000000", r.formatNumber(0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.500000", r.formatNumber(1).c_str());
    TEST_ASSERT_EQUAL_STRING("5.000000", r.formatNumber(10).c_str());
    TEST_ASSERT_EQUAL_STRING("-500.500000", r.formatNumber(-1001l).c_str());
}

TEST_CASE("decodeData", "[nibegw_config]") {
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    uint8_t data1234[] = {1, 2, 3, 4};
    uint8_t dataFF[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_STRING("1", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("255", r.decodeData(dataFF).c_str());

    r.dataType = NibeRegisterDataType::Int8;
    TEST_ASSERT_EQUAL_STRING("1", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", r.decodeData(dataFF).c_str());

    r.dataType = NibeRegisterDataType::UInt16;
    TEST_ASSERT_EQUAL_STRING("513", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("65535", r.decodeData(dataFF).c_str());

    r.dataType = NibeRegisterDataType::Int16;
    TEST_ASSERT_EQUAL_STRING("513", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", r.decodeData(dataFF).c_str());

    r.dataType = NibeRegisterDataType::UInt32;
    TEST_ASSERT_EQUAL_STRING("67305985", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("4294967295", r.decodeData(dataFF).c_str());

    r.dataType = NibeRegisterDataType::Int32;
    TEST_ASSERT_EQUAL_STRING("67305985", r.decodeData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", r.decodeData(dataFF).c_str());

    // not yet implemented types
    r.dataType = NibeRegisterDataType::Unknown;
    TEST_ASSERT_EQUAL_STRING("", r.decodeData(data1234).c_str());
}

void assertNibeRegisterData(const uint8_t actual[4], uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    TEST_ASSERT_EQUAL_UINT8(byte1, actual[0]);
    TEST_ASSERT_EQUAL_UINT8(byte2, actual[1]);
    TEST_ASSERT_EQUAL_UINT8(byte3, actual[2]);
    TEST_ASSERT_EQUAL_UINT8(byte4, actual[3]);
}

TEST_CASE("encodeData", "[nibegw_config]") {
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    uint8_t data[4] = {0, 0, 0, 0};

    TEST_ASSERT_TRUE(r.encodeData("1", data));
    assertNibeRegisterData(data, 1, 0, 0, 0);
    TEST_ASSERT_TRUE(r.encodeData("255", data));
    assertNibeRegisterData(data, 0xff, 0, 0, 0);

    r.dataType = NibeRegisterDataType::Int8;
    TEST_ASSERT_TRUE(r.encodeData("1", data));
    assertNibeRegisterData(data, 1, 0, 0, 0);
    TEST_ASSERT_TRUE(r.encodeData("-1", data));
    assertNibeRegisterData(data, 0xff, 0xff, 0xff, 0xff);

    r.dataType = NibeRegisterDataType::UInt16;
    TEST_ASSERT_TRUE(r.encodeData("513", data));
    assertNibeRegisterData(data, 1, 2, 0, 0);
    TEST_ASSERT_TRUE(r.encodeData("65535", data));
    assertNibeRegisterData(data, 0xff, 0xff, 0, 0);

    r.dataType = NibeRegisterDataType::Int16;
    TEST_ASSERT_TRUE(r.encodeData("513", data));
    assertNibeRegisterData(data, 1, 2, 0, 0);
    TEST_ASSERT_TRUE(r.encodeData("-1", data));
    assertNibeRegisterData(data, 0xff, 0xff, 0xff, 0xff);

    r.dataType = NibeRegisterDataType::UInt32;
    TEST_ASSERT_TRUE(r.encodeData("67305985", data));
    assertNibeRegisterData(data, 1, 2, 3, 4);
    TEST_ASSERT_TRUE(r.encodeData("4294967295", data));
    assertNibeRegisterData(data, 0xff, 0xff, 0xff, 0xff);

    r.dataType = NibeRegisterDataType::Int32;
    TEST_ASSERT_TRUE(r.encodeData("67305985", data));
    assertNibeRegisterData(data, 1, 2, 3, 4);
    TEST_ASSERT_TRUE(r.encodeData("-1", data));
    assertNibeRegisterData(data, 0xff, 0xff, 0xff, 0xff);

    // bad data
    TEST_ASSERT_FALSE(r.encodeData("", data));
    TEST_ASSERT_FALSE(r.encodeData("x", data));

    // not yet implemented types
    r.dataType = NibeRegisterDataType::Unknown;
    TEST_ASSERT_FALSE(r.encodeData("1", data));
}

void testParseSignedNumberException(const NibeRegister& r, const char* number) {
    try {
        r.parseSignedNumber(number);
        TEST_FAIL_MESSAGE("Expected std::invalid_argument exception");
    } catch (std::invalid_argument& e) {
        // expected
    } catch (...) {
        // linux target: throws std::invalid_argument but it is not caught by the catch block above
        // libc++abi: terminating due to uncaught exception of type std::invalid_argument: stoi: no conversion
    }
}

TEST_CASE("parseSignedNumber", "[nibegw_config]") {
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};

    TEST_ASSERT_EQUAL(0, r.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("1.1"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("10"));
    TEST_ASSERT_EQUAL(-1000, r.parseSignedNumber("-1000"));
    // bad number format
    testParseSignedNumberException(r, "x");
    testParseSignedNumberException(r, "");

    r.factor = 10;
    TEST_ASSERT_EQUAL(0, r.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("0.1"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("1.0"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("1.00"));
    TEST_ASSERT_EQUAL(-1001, r.parseSignedNumber("-100.1"));
    TEST_ASSERT_EQUAL(-1001, r.parseSignedNumber("-100.123"));
    // bad number format
    testParseSignedNumberException(r, "x");
    testParseSignedNumberException(r, "");

    r.factor = 100;
    TEST_ASSERT_EQUAL(0, r.parseSignedNumber("0.00"));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("0.01"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("0.10"));
    TEST_ASSERT_EQUAL(100, r.parseSignedNumber("1"));
    TEST_ASSERT_EQUAL(100, r.parseSignedNumber("1."));
    TEST_ASSERT_EQUAL(100, r.parseSignedNumber("1.0"));
    TEST_ASSERT_EQUAL(100, r.parseSignedNumber("1.00"));
    TEST_ASSERT_EQUAL(100, r.parseSignedNumber("1.000"));
    TEST_ASSERT_EQUAL(-1005, r.parseSignedNumber("-10.05"));
    TEST_ASSERT_EQUAL(-1005, r.parseSignedNumber("-10.0599"));
    // bad number format
    testParseSignedNumberException(r, "x");
    testParseSignedNumberException(r, "");

    r.factor = 2;
    TEST_ASSERT_EQUAL(0, r.parseSignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseSignedNumber("0.5"));
    TEST_ASSERT_EQUAL(10, r.parseSignedNumber("5.0"));
    TEST_ASSERT_EQUAL(-1001, r.parseSignedNumber("-500.5000"));
    // bad number format
    testParseSignedNumberException(r, "x");
    testParseSignedNumberException(r, "");
}

void testParseUnsignedNumberException(const NibeRegister& c, const char* number) {
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
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};

    TEST_ASSERT_EQUAL(0, r.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("1.1"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("10"));
    TEST_ASSERT_EQUAL(4294967295, r.parseUnsignedNumber("4294967295"));
    TEST_ASSERT_EQUAL(4294967295, r.parseUnsignedNumber("-1"));  // not exactly nice but that's how std::stoul works
    // bad number format
    testParseUnsignedNumberException(r, "x");
    testParseUnsignedNumberException(r, "");

    r.factor = 10;
    TEST_ASSERT_EQUAL(0, r.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("0.1"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("1.0"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("1.00"));
    TEST_ASSERT_EQUAL(4294967290, r.parseUnsignedNumber("429496729"));
    // bad number format
    testParseUnsignedNumberException(r, "x");
    testParseUnsignedNumberException(r, "");

    r.factor = 100;
    TEST_ASSERT_EQUAL(0, r.parseUnsignedNumber("0.00"));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("0.01"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("0.10"));
    TEST_ASSERT_EQUAL(100, r.parseUnsignedNumber("1"));
    TEST_ASSERT_EQUAL(100, r.parseUnsignedNumber("1."));
    TEST_ASSERT_EQUAL(100, r.parseUnsignedNumber("1.0"));
    TEST_ASSERT_EQUAL(100, r.parseUnsignedNumber("1.00"));
    TEST_ASSERT_EQUAL(100, r.parseUnsignedNumber("1.000"));
    TEST_ASSERT_EQUAL(4294967200, r.parseUnsignedNumber("42949672"));
    // bad number format
    testParseUnsignedNumberException(r, "x");
    testParseUnsignedNumberException(r, "");

    r.factor = 2;
    TEST_ASSERT_EQUAL(0, r.parseUnsignedNumber("0"));
    TEST_ASSERT_EQUAL(1, r.parseUnsignedNumber("0.5"));
    TEST_ASSERT_EQUAL(10, r.parseUnsignedNumber("5.0"));
    TEST_ASSERT_EQUAL(4294967295, r.parseUnsignedNumber("2147483647"));  // precision problem
    // bad number format
    testParseUnsignedNumberException(r, "x");
    testParseUnsignedNumberException(r, "");
}

TEST_CASE("homeassistantDiscoveryMessage Temperature", "[nibegw_config]") {
    NibeMqttConfig config;
    NibeRegister r = {1, "Temperature",         NibeRegisterUnit::GradCelcius, NibeRegisterDataType::UInt8, 1, 0, 0,
                      0, NibeRegisterMode::Read};
    std::string deviceDiscoveryInfo = R"("dev":{"name":"Nibe GW"})";
    auto doc = r.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

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
    NibeRegister r = {1, "No Unit", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = r.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

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
    NibeRegister r = {
        1, "Temperature", NibeRegisterUnit::GradCelcius, NibeRegisterDataType::UInt8, 10, 0, 100, 0, NibeRegisterMode::ReadWrite};
    std::string deviceDiscoveryInfo = R"("dev":{"name":"Nibe GW"})";
    auto doc = r.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

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
    NibeRegister r = {1, "Override", NibeRegisterUnit::GradCelcius, NibeRegisterDataType::UInt8, 1, 0,
                      0, 0,          NibeRegisterMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = r.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

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
    NibeRegister r = {43005, "Degree Minutes",      NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0,
                      0,     NibeRegisterMode::Read};
    std::string deviceDiscoveryInfo = R"("device":{"name":"Nibe GW"})";
    auto doc = r.homeassistantDiscoveryMessage(config, "nibegw/coils/", deviceDiscoveryInfo);

    TEST_ASSERT_EQUAL_STRING("sensor", doc["_component_"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["obj_id"]);
    TEST_ASSERT_EQUAL_STRING("nibegw-coil-43005", doc["uniq_id"]);
    TEST_ASSERT_EQUAL_STRING("Degree Minutes", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("nibegw/coils/43005", doc["stat_t"]);
    TEST_ASSERT_TRUE(doc["unit_of_meas"].isUnbound());
    TEST_ASSERT_EQUAL_STRING("measurement", doc["stat_cla"]);
    TEST_ASSERT_EQUAL_STRING("Nibe GW", doc["device"]["name"]);
}

TEST_CASE("decodeDataRaw", "[nibegw_config]") {
    NibeRegister r = {0, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    uint8_t data1234[] = {1, 2, 3, 4};
    uint8_t dataFF[] = {0xff, 0xff, 0xff, 0xff};
    TEST_ASSERT_EQUAL_UINT8(1, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT8(255, r.decodeDataRaw(dataFF));

    r.dataType = NibeRegisterDataType::Int8;
    TEST_ASSERT_EQUAL_INT8(1, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT8(-1, r.decodeDataRaw(dataFF));

    r.dataType = NibeRegisterDataType::UInt16;
    TEST_ASSERT_EQUAL_UINT16(513, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT16(65535, r.decodeDataRaw(dataFF));

    r.dataType = NibeRegisterDataType::Int16;
    TEST_ASSERT_EQUAL_INT16(513, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT16(-1, r.decodeDataRaw(dataFF));

    r.dataType = NibeRegisterDataType::UInt32;
    TEST_ASSERT_EQUAL_UINT32(67305985, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_UINT32(4294967295, r.decodeDataRaw(dataFF));

    r.dataType = NibeRegisterDataType::Int32;
    TEST_ASSERT_EQUAL_INT32(67305985, r.decodeDataRaw(data1234));
    TEST_ASSERT_EQUAL_INT32(-1, r.decodeDataRaw(dataFF));

    // not yet implemented types
    r.dataType = NibeRegisterDataType::Unknown;
    TEST_ASSERT_EQUAL(0, r.decodeDataRaw(data1234));
}

TEST_CASE("promMetricName", "[nibegw_config]") {
    NibeRegister r = {1, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    TEST_ASSERT_EQUAL_STRING(R"(nibe)", r.promMetricName().c_str());
    r.title = "Test123";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_Test123)", r.promMetricName().c_str());
    r.title = "Test 123";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_Test_123)", r.promMetricName().c_str());
    r.title = "1Test";
    TEST_ASSERT_EQUAL_STRING(R"(nibe_1Test)", r.promMetricName().c_str());
}

TEST_CASE("appendPromAttributes", "[nibegw_config]") {
    NibeRegister r = {1, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    std::string s = "test";
    r.appendPromAttributes(s);
    TEST_ASSERT_EQUAL_STRING(R"(test{coil="1"})", s.c_str());

    s = R"(test{attr="value"})";
    r.appendPromAttributes(s);
    TEST_ASSERT_EQUAL_STRING(R"(test{coil="1",attr="value"})", s.c_str());
}

TEST_CASE("toPromMetricConfig", "[nibegw_config]") {
    NibeRegister r = {1, "", NibeRegisterUnit::NoUnit, NibeRegisterDataType::UInt8, 1, 0, 0, 0, NibeRegisterMode::Read};
    NibeMqttConfig config;
    config.metrics[1] = {"test123", 10, 1, false};
    config.metrics[2] = {"", 10, 10, false};
    config.metrics[3] = {R"(test123{node="nibegw"})", 0, 0, false};

    NibeRegisterMetricConfig metricCfg = r.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(test123{coil="1"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(10, metricCfg.factor);
    TEST_ASSERT_EQUAL(1, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    r.id = 2;
    metricCfg = r.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(nibe{coil="2"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(10, metricCfg.factor);
    TEST_ASSERT_EQUAL(10, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    r.id = 3;
    metricCfg = r.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING(R"(test123{coil="3",node="nibegw"})", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(1, metricCfg.factor);
    TEST_ASSERT_EQUAL(1, metricCfg.scale);
    TEST_ASSERT_TRUE(metricCfg.isValid());

    // metric not in config -> do not create a valid metric
    r.id = 4;
    metricCfg = r.toPromMetricConfig(config);
    TEST_ASSERT_EQUAL_STRING("", metricCfg.name.c_str());
    TEST_ASSERT_EQUAL(0, metricCfg.factor);
    TEST_ASSERT_EQUAL(0, metricCfg.scale);
    TEST_ASSERT_FALSE(metricCfg.isValid());
}