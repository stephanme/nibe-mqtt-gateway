#include <unity.h>

#include "nibegw_config.h"

TEST_CASE("formatNumber", "[nibegw_config]") {
    Coil c = {0, "", "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
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
    Coil c = {0, "", "", CoilUnit::NoUnit, CoilDataType::UInt8, 1, 0, 0, 0, CoilMode::Read};
    NibeReadResponseData data1234 = {.coilAddress = 0, .value = {1, 2, 3, 4}};
    NibeReadResponseData dataFF = {.coilAddress = 0, .value = {0xff, 0xff, 0xff, 0xff}};
    TEST_ASSERT_EQUAL_STRING("1", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("255", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int8;
    TEST_ASSERT_EQUAL_STRING("1", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::UInt16;
    TEST_ASSERT_EQUAL_STRING("258", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("65535", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int16;
    TEST_ASSERT_EQUAL_STRING("258", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::UInt32;
    TEST_ASSERT_EQUAL_STRING("16909060", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("4294967295", c.decodeCoilData(dataFF).c_str());

    c.dataType = CoilDataType::Int32;
    TEST_ASSERT_EQUAL_STRING("16909060", c.decodeCoilData(data1234).c_str());
    TEST_ASSERT_EQUAL_STRING("-1", c.decodeCoilData(dataFF).c_str());

    // not yet implemented types
    c.dataType = CoilDataType::Unknown;
    TEST_ASSERT_EQUAL_STRING("", c.decodeCoilData(data1234).c_str());
}