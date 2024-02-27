#include <unity.h>

#include <bit>

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
    uint8_t data[] = {0xc0, 0x69, 0x11, 0x22, 0xFF};
    NibeReadRequestMessage* request = (NibeReadRequestMessage*)data;
    TEST_ASSERT_EQUAL_HEX8(NIBE_REQUEST_START, request->start);
    TEST_ASSERT_EQUAL_HEX8(NIBE_CMD_MODBUS_READ_REQ, request->cmd);
    TEST_ASSERT_EQUAL_HEX16(0x1122, std::byteswap(request->coilAddress));
    TEST_ASSERT_EQUAL_HEX8(0xFF, request->chksum);
}

TEST_CASE("NibeResponseMessage", "[nibegw]") {
    uint8_t data[] = {0x5c, 0x00, 0x20, 0x6a, 0x04, 0x11, 0x22, 0x01, 0x02, 0xFF};
    NibeResponseMessage* response = (NibeResponseMessage*)data;
    TEST_ASSERT_EQUAL_HEX8(NIBE_RESPONSE_START, response->start);
    TEST_ASSERT_EQUAL_HEX16(NIBE_ADDRESS_MODBUS, std::byteswap(response->address));
    TEST_ASSERT_EQUAL_HEX8(NIBE_CMD_MODBUS_READ_RESP, response->cmd);
    TEST_ASSERT_EQUAL(4, response->len);
    TEST_ASSERT_EQUAL_HEX16(0x1122, std::byteswap(response->readResponse.coilAddress));
    TEST_ASSERT_EQUAL_HEX8(0x01, response->readResponse.value[0]);
    TEST_ASSERT_EQUAL_HEX8(0x02, response->readResponse.value[1]);
}

TEST_CASE("dataToString", "[nibegw]") {
    uint8_t data[] = {0x12, 0x34, 0xCD, 0xEF};
    TEST_ASSERT_EQUAL_STRING("", AbstractNibeGw::dataToString(data, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 ", AbstractNibeGw::dataToString(data, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 CD EF ", AbstractNibeGw::dataToString(data, 4).c_str());
}