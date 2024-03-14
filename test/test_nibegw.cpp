#include <unity.h>

#include "nibegw.h"

TEST_CASE("calcCheckSum", "[nibegw]") {
    uint8_t data[] = {0x1, 0x2, 0x3, 0x4};
    TEST_ASSERT_EQUAL_HEX8(0x00, AbstractNibeGw::calcCheckSum(data, 0));
    TEST_ASSERT_EQUAL_HEX8(0x01, AbstractNibeGw::calcCheckSum(data, 1));
    TEST_ASSERT_EQUAL_HEX8(0x03, AbstractNibeGw::calcCheckSum(data, 2));
    TEST_ASSERT_EQUAL_HEX8(0x00, AbstractNibeGw::calcCheckSum(data, 3));
    TEST_ASSERT_EQUAL_HEX8(0x04, AbstractNibeGw::calcCheckSum(data, 4));
}

TEST_CASE("NibeReadRequestMessage", "[nibegw]") {
    uint8_t data[] = {0xc0, 0x69, 0x02, 0x44, 0x9c, 0x73};
    NibeReadRequestMessage* request = (NibeReadRequestMessage*)data;
    TEST_ASSERT_EQUAL_HEX8(NibeStart::Request, request->start);
    TEST_ASSERT_EQUAL_HEX8(NibeCmd::ModbusReadReq, request->cmd);
    TEST_ASSERT_EQUAL_HEX8(2, request->len);
    TEST_ASSERT_EQUAL_HEX16(40004, request->coilAddress);
    TEST_ASSERT_EQUAL_HEX8(0x73, request->chksum);

    uint8_t data2[] = {0xc0, 0x69, 0x02, 0xa0, 0xa9, 0xa2};
    request = (NibeReadRequestMessage*)data2;
    TEST_ASSERT_EQUAL_HEX8(NibeStart::Request, request->start);
    TEST_ASSERT_EQUAL_HEX8(NibeCmd::ModbusReadReq, request->cmd);
    TEST_ASSERT_EQUAL_HEX8(2, request->len);
    TEST_ASSERT_EQUAL_HEX16(43424, request->coilAddress);
    TEST_ASSERT_EQUAL_HEX8(0xa2, request->chksum);
}

TEST_CASE("NibeResponseMessage", "[nibegw]") {
    uint8_t data[] = {0x5C, 0x00, 0x20, 0x6A, 0x06, 0x44, 0x9C, 0x6E, 0x00, 0x00, 0x80, 0x7A};
    NibeResponseMessage* response = (NibeResponseMessage*)data;
    TEST_ASSERT_EQUAL_HEX8(NibeStart::Response, response->start);
    TEST_ASSERT_EQUAL_HEX16(NibeDeviceAddress::MODBUS40, response->deviceAddress);
    TEST_ASSERT_EQUAL_HEX8(NibeCmd::ModbusReadResp, response->cmd);
    TEST_ASSERT_EQUAL(6, response->len);
    TEST_ASSERT_EQUAL_HEX16(40004, response->readResponse.coilAddress);
    TEST_ASSERT_EQUAL_HEX8(0x6e, response->readResponse.value[0]);

    uint8_t data2[] = {0x5c, 0x00, 0x20, 0x6a, 0x06, 0xa0, 0xa9, 0xf5, 0x12, 0x00, 0x00, 0xa2};
    response = (NibeResponseMessage*)data2;
    TEST_ASSERT_EQUAL_HEX8(NibeStart::Response, response->start);
    TEST_ASSERT_EQUAL_HEX16(NibeDeviceAddress::MODBUS40, response->deviceAddress);
    TEST_ASSERT_EQUAL_HEX8(NibeCmd::ModbusReadResp, response->cmd);
    TEST_ASSERT_EQUAL(6, response->len);
    TEST_ASSERT_EQUAL_HEX16(43424, response->readResponse.coilAddress);
    TEST_ASSERT_EQUAL_HEX8(0xf5, response->readResponse.value[0]);
}

TEST_CASE("dataToString", "[nibegw]") {
    uint8_t data[] = {0x12, 0x34, 0xCD, 0xEF};
    TEST_ASSERT_EQUAL_STRING("", AbstractNibeGw::dataToString(data, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 ", AbstractNibeGw::dataToString(data, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 CD EF ", AbstractNibeGw::dataToString(data, 4).c_str());
}