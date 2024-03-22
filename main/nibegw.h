/**
 * Copyright (c) 2010-2024 Contributors to the openHAB project
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * ----------------------------------------------------------------------------
 *
 * Frame format:
 * +----+------+------+-----+-----+----+----+-----+
 * | 5C | ADDR | ADDR | CMD | LEN |  DATA   | CHK |
 * +----+------+------+-----+-----+----+----+-----+
 *
 *      |------------ CHK ------------------|
 *
 *  Address:
 *    0x0016 = SMS40
 *    0x0019 = RMU40
 *    0x0020 = MODBUS40
 *
 *  Checksum: XOR
 *
 * When valid data is received (checksum ok),
 *  ACK (0x06) should be sent to the heat pump.
 * When checksum mismatch,
 *  NAK (0x15) should be sent to the heat pump.
 *
 * Original Author: pauli.anttila@gmail.com
 *
 */

// Based on https://github.com/openhab/openhab-addons/commit/f4596f581e45f42a74fca4094ba0f40b39dbdf2c
// Changes:
// - reformatted
// - use ESP_LOG, removed debug callback
// - removed unused HW support
// - typesafe callback via interface
// - own task that handles the RS485 loop
// - separated into nibegw and nibgw_rs485 to get rid of Arduino.h for testing on linux
// - bigger refactoring to make NibeGW testable, separated NibeGW and NibeInterface

#ifndef _nibegw_h_
#define _nibegw_h_

#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>

#include <string>

// state machine states
enum eState {
    STATE_WAIT_START,
    STATE_WAIT_DATA,
    STATE_OK_MESSAGE_RECEIVED,
    STATE_CRC_FAILURE,
};

#define NIBE_GW_TASK_STACK_SIZE 10 * 1024
#define NIBE_GW_TASK_PRIORITY 15

// message buffer for RS-485 communication. Max message length is 80 uint8_ts + 6 uint8_ts header
#define MAX_DATA_LEN 128

// nibe protocol data structures work on little endian processors only, big endian would need byteswap which is not implemented
// ESP32 (Tensilica Xtensa LX6) is little endian
// ARM Mac as well (used for testing with target linux)

enum class NibeStart : uint8_t {
    Response = 0x5c,
    Request = 0xc0,
};

// data format might be big endian, transformed to little endian by swapping the address constants
enum class NibeDeviceAddress : uint16_t {
    SMS40 = 0x1600,
    RMU40 = 0x1900,
    MODBUS40 = 0x2000,
    Heatpump1 = 0xC941,
};

enum class NibeCmd : u_int8_t {
    ModbusDataMsg = 0x68,
    ModbusReadReq = 0x69,
    ModbusReadResp = 0x6A,
    ModbusWriteReq = 0x6B,
    ModbusWriteResp = 0x6C,
    ProductInfoMsg = 0x6D,
    ModbusAddressMsg = 0x6E,
    AccessoryVersionReq = 0xEE,
};

struct __attribute__((packed)) NibeReadRequestMessage {
    NibeStart start;  // const 0xc0
    NibeCmd cmd;      // NibeCmd
    uint8_t len;      // = 2
    uint16_t coilAddress;
    uint8_t chksum;  // xor of start..coilAddress
};

struct __attribute__((packed)) NibeWriteRequestMessage {
    NibeStart start;  // const 0xc0
    NibeCmd cmd;      // NibeCmd
    uint8_t len;      // = 6
    uint16_t coilAddress;
    uint8_t value[4];
    uint8_t chksum;  // xor of start..value[]
};

struct __attribute__((packed)) NibeReadResponseData {
    uint16_t coilAddress;
    uint8_t value[4];  // unclear if always 4 bytes or depends on coil address/type
};

struct __attribute__((packed)) NibeResponseMessage {
    NibeStart start;                  // const 0x5c
    NibeDeviceAddress deviceAddress;  // NibeDeviceAddress
    NibeCmd cmd;                      // NibeCmd
    uint8_t len;
    union {
        uint8_t data[1];                    // len bytes
        NibeReadResponseData readResponse;  // NibeCmd == ModbusReadResp
    };
    // uint8 chksum; // xor of address..data
};

class NibeGwCallback {
   public:
    virtual void onMessageReceived(const NibeResponseMessage* const data, int len) = 0;
    virtual int onReadTokenReceived(NibeReadRequestMessage* data) = 0;
    virtual int onWriteTokenReceived(NibeWriteRequestMessage* data) = 0;
};

class NibeInterface {
   public:
    // check if data can be read (non-blocking)
    virtual bool isDataAvailable() = 0;
    // read data from interface, returns -1 if no data is available
    virtual int readData() = 0;

    virtual void sendData(const uint8_t* const data, uint8_t len) = 0;
    virtual void sendData(const uint8_t data) = 0;
};

class NibeGw {
   public:
    NibeGw(NibeInterface& nibeInterface);

    esp_err_t begin(NibeGwCallback& callback);
    // TODO: begin for testing w/o starting a task

    static uint8_t calcCheckSum(const uint8_t* const data, uint8_t len) {
        uint8_t chksum = 0;
        for (int i = 0; i < len; i++) {
            chksum ^= data[i];
        }
        return chksum;
    }

    // for logging and debugging
    static std::string dataToString(const uint8_t* const data, int len);

    // for testing
    esp_err_t beginTest(NibeGwCallback& callback);
    void loop();
    auto getState() { return state; }

   private:
    NibeInterface& nibeInterface;

    // protocol handling
    NibeGwCallback* callback;
    eState state;
    uint8_t buffer[MAX_DATA_LEN];
    const NibeResponseMessage* bufferAsMsg = (NibeResponseMessage*)buffer;
    uint8_t index;

    static int checkNibeMessage(const uint8_t* const data, uint8_t len);
    void sendResponseMessage(int len);
    void sendAck();
    void sendNak();
    bool shouldAckNakSend(NibeDeviceAddress address);

    static void task(void* pvParameters);
};

#endif