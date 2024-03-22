#include <esp_log.h>
#include <unity.h>

#include "nibegw.h"

static const char* TAG = "test_nibegw";

class NibeMockInterface : public NibeInterface {
   public:
    NibeMockInterface() {
        readBufferIndex = 0;
        sendBufferIndex = 0;
    }

    void setReadData(const uint8_t* const data, uint8_t len);
    bool checkWriteData(const uint8_t* const data, uint8_t len);

    // NibeInterface
    virtual bool isDataAvailable();
    virtual int readData();
    virtual void sendData(const uint8_t* const data, uint8_t len);
    virtual void sendData(const uint8_t data);

   protected:
    uint8_t readBuffer[MAX_DATA_LEN];
    uint8_t readBufferIndex;
    uint8_t sendBuffer[MAX_DATA_LEN];
    uint8_t sendBufferIndex;
};

// NibeMockInterface

void NibeMockInterface::setReadData(const uint8_t* const data, uint8_t len) {
    // forgets all former content
    // store in reverse order so that first data byte is read first
    readBufferIndex = 0;
    for (int i = len - 1; i >= 0; i--) {
        readBuffer[readBufferIndex++] = data[i];
    }
    ESP_LOGD(TAG, "Append %s", NibeGw::dataToString(data, len).c_str());
}

bool NibeMockInterface::checkWriteData(const uint8_t* const data, uint8_t len) {
    if (sendBufferIndex != len) return false;
    for (int i = 0; i < len; i++) {
        if (sendBuffer[i] != data[i]) return false;
    }
    return true;
}

bool NibeMockInterface::isDataAvailable() { return readBufferIndex > 0; }

int NibeMockInterface::readData() {
    int ret = -1;
    if (readBufferIndex > 0) {
        ret = readBuffer[--readBufferIndex];
    }
    return ret;
}

void NibeMockInterface::sendData(const uint8_t* const data, uint8_t len) {
    std::memcpy(sendBuffer, data, len);
    sendBufferIndex = len;
    ESP_LOGD(TAG, "Send %s", NibeGw::dataToString(data, len).c_str());
}

void NibeMockInterface::sendData(const uint8_t data) {
    sendBuffer[0] = data;
    sendBufferIndex = 1;
    ESP_LOGD(TAG, "Send %02x", data);
}

class NibeMockCallback : public NibeGwCallback {
   public:
    NibeMockCallback() { reset(); }

    virtual void onMessageReceived(const NibeResponseMessage* const data, int len) {
        onMessageReceivedCnt++;
        lastMessageReceivedLen = len;
        lastMessageReceived = data;
    }
    virtual int onReadTokenReceived(NibeReadRequestMessage* data) {
        onReadTokenReceivedCnt++;
        return 0;
    }
    virtual int onWriteTokenReceived(NibeWriteRequestMessage* data) {
        onWriteTokenReceivedCnt++;
        return 0;
    }

    void reset() {
        onReadTokenReceivedCnt = 0;
        onWriteTokenReceivedCnt = 0;
        onMessageReceivedCnt = 0;
        lastMessageReceived = nullptr;
        lastMessageReceivedLen = 0;
    }

    int onReadTokenReceivedCnt;
    int onWriteTokenReceivedCnt;
    int onMessageReceivedCnt;

    const NibeResponseMessage* lastMessageReceived;
    int lastMessageReceivedLen;
};

TEST_CASE("calcCheckSum", "[nibegw]") {
    uint8_t data[] = {0x1, 0x2, 0x3, 0x4};
    TEST_ASSERT_EQUAL_HEX8(0x00, NibeGw::calcCheckSum(data, 0));
    TEST_ASSERT_EQUAL_HEX8(0x01, NibeGw::calcCheckSum(data, 1));
    TEST_ASSERT_EQUAL_HEX8(0x03, NibeGw::calcCheckSum(data, 2));
    TEST_ASSERT_EQUAL_HEX8(0x00, NibeGw::calcCheckSum(data, 3));
    TEST_ASSERT_EQUAL_HEX8(0x04, NibeGw::calcCheckSum(data, 4));
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
    TEST_ASSERT_EQUAL_STRING("", NibeGw::dataToString(data, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 ", NibeGw::dataToString(data, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("12 34 CD EF ", NibeGw::dataToString(data, 4).c_str());
}

TEST_CASE("read token", "[nibegw]") {
    NibeMockInterface interface;
    NibeMockCallback callback;
    NibeGw gw(interface);
    gw.setNibeGwCallback(callback);

    uint8_t data[] = {0x5C, 0x00, 0x20, 0x69, 0x00, 0x49};
    // data[sizeof(data) - 1] = NibeGw::calcCheckSum(data + 1, sizeof(data) - 2);
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_OK_MESSAGE_RECEIVED, gw.getState());
    gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());

    TEST_ASSERT_EQUAL(1, callback.onReadTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onWriteTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onMessageReceivedCnt);
    // check ACK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){0x06}, 1));
}

TEST_CASE("read response", "[nibegw]") {
    NibeMockInterface interface;
    NibeMockCallback callback;
    NibeGw gw(interface);
    gw.setNibeGwCallback(callback);

    uint8_t data[] = {0x5C, 0x00, 0x20, 0x6A, 0x06, 0x44, 0x9C, 0x6E, 0x00, 0x00, 0x80, 0x7A};
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_OK_MESSAGE_RECEIVED, gw.getState());
    gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());

    TEST_ASSERT_EQUAL(0, callback.onReadTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onWriteTokenReceivedCnt);
    TEST_ASSERT_EQUAL(1, callback.onMessageReceivedCnt);

    TEST_ASSERT_EQUAL(12, callback.lastMessageReceivedLen);
    TEST_ASSERT_EQUAL(6, callback.lastMessageReceived->len);
    TEST_ASSERT_EQUAL_HEX16(40004, callback.lastMessageReceived->readResponse.coilAddress);
    TEST_ASSERT_EQUAL_HEX8(0x6e, callback.lastMessageReceived->readResponse.value[0]);

    // check ACK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){0x06}, 1));
}

TEST_CASE("non-modbus address", "[nibegw]") {
    NibeMockInterface interface;
    NibeMockCallback callback;
    NibeGw gw(interface);
    gw.setNibeGwCallback(callback);

    uint8_t data[] = {0x5C, 0x41, 0xC9, 0x69, 0x00, 0xE1};
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_OK_MESSAGE_RECEIVED, gw.getState());
    gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());

    TEST_ASSERT_EQUAL(0, callback.onReadTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onWriteTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onMessageReceivedCnt);
    // no ACK/NAK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){}, 0));
}

TEST_CASE("wrong CRC", "[nibegw]") {
    NibeMockInterface interface;
    NibeMockCallback callback;
    NibeGw gw(interface);
    gw.setNibeGwCallback(callback);

    uint8_t data[] = {0x5C, 0x00, 0x20, 0x69, 0x00, 0xFF};
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_CRC_FAILURE, gw.getState());
    gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());

    TEST_ASSERT_EQUAL(0, callback.onReadTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onWriteTokenReceivedCnt);
    TEST_ASSERT_EQUAL(0, callback.onMessageReceivedCnt);
    // NAK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){ 0x15 }, 1));
}

TEST_CASE("find response start", "[nibegw]") {
    NibeMockInterface interface;
    NibeMockCallback callback;
    NibeGw gw(interface);
    gw.setNibeGwCallback(callback);

    uint8_t data[] = {0x00, 0x01, 0x22, 0x5C, 0x00, 0x20, 0x69, 0x00, 0x49, 0x99, 0xaa};
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());

    TEST_ASSERT_EQUAL(1, callback.onReadTokenReceivedCnt);
    // check ACK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){0x06}, 1));
}

TEST_CASE("protocol handling w/o callback", "[nibegw]") {
    NibeMockInterface interface;
    NibeGw gw(interface);

    uint8_t data[] = {0x00, 0x01, 0x22, 0x5C, 0x00, 0x20, 0x69, 0x00, 0x49, 0x99, 0xaa};
    interface.setReadData(data, sizeof(data));

    while (interface.isDataAvailable()) gw.loop();
    TEST_ASSERT_EQUAL(eState::STATE_WAIT_START, gw.getState());
    // check ACK
    TEST_ASSERT_TRUE(interface.checkWriteData((uint8_t[]){0x06}, 1));
}
