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
 * Author: pauli.anttila@gmail.com
 *
 */

// Based on https://github.com/openhab/openhab-addons/commit/f4596f581e45f42a74fca4094ba0f40b39dbdf2c
// Changes:
// - reformatted
// - use ESP_LOG, removed debug callback
// - removed unused HW support
// - typesafe callback via interface
// - own task that handles the RS485 loop

#ifndef NibeGw_h
#define NibeGw_h

#include <Arduino.h>

// state machine states
enum eState {
    STATE_WAIT_START,
    STATE_WAIT_DATA,
    STATE_OK_MESSAGE_RECEIVED,
    STATE_CRC_FAILURE,
};

#define NIBE_GW_TASK_PRIORITY 15

// message buffer for RS-485 communication. Max message length is 80 bytes + 6 bytes header
#define MAX_DATA_LEN 128

#define SMS40 0x16
#define RMU40 0x19
#define MODBUS40 0x20

// nibe data formats
// ESP32 (Tensilica Xtensa LX6) is little endian
// nibe seems to use big endian

enum NibeStart {
    NIBE_RESPONSE_START = 0x5c,
    NIBE_REQUEST_START = 0xc0,
};

enum NibeAddress {
    NIBE_ADDRESS_SMS40 = SMS40,
    NIBE_ADDRESS_RMU40 = RMU40,
    NIBE_ADDRESS_MODBUS = MODBUS40,
};

enum NibeCmd {
    NIBE_CMD_MODBUS_DATA_MSG=0x68,
    NIBE_CMD_MODBUS_READ_REQ=0x69,
    NIBE_CMD_MODBUS_READ_RESP=0x6A,
    NIBE_CMD_MODBUS_WRITE_REQ=0x6B,
    NIBE_CMD_MODBUS_WRITE_RESP=0x6C,
    NIBE_CMD_MODBUS_ADDRESS_MSG=0x6E,
};

struct __attribute__ ((packed)) NibeReadRequestMessage {
    uint8_t start; // const 0xc0
    uint8_t cmd; // NibeCmd
    uint16_t coilAddress;    
    uint8_t chksum; // xor of start..coilAddress
};

struct __attribute__ ((packed)) NibeWriteRequestMessage {
    uint8_t start; // const 0xc0
    uint8_t cmd; // NibeCmd
    uint16_t coilAddress;
    uint8_t value[4];
    uint8_t chksum; // xor of start..value[]
};

struct __attribute__ ((packed)) NibeReadResponseData {
    uint16_t coilAddress;
    uint8_t value[4];  // unclear if always 4 bytes or depends on coil address/type
};

struct __attribute__ ((packed)) NibeResponseMessage {
    uint8_t start; // const 0x5c
    uint16_t address; // NibeAddress
    uint8_t cmd; // NibeCmd
    uint8_t len;
    union {
        uint8_t data[1]; // len bytes
        NibeReadResponseData readResponse; // NibeCmd == NIBE_CMD_MODBUS_READ_RESP
    };
    // uint8 chksum; // xor of address..data
};

class NibeGwCallback {
   public:
    virtual void onMessageReceived(const byte* const data, int len) = 0;
    virtual int onReadTokenReceived(byte* data) = 0;
    virtual int onWriteTokenReceived(byte* data) = 0;
};


class AbstractNibeGw {
   public:
    virtual esp_err_t begin(NibeGwCallback& callback) = 0;

    static uint8_t calcCheckSum(const byte* const data, byte len) {
        uint8_t chksum = 0;
        for (int i = 0; i < len; i++) {
            chksum ^= data[i];
        }
        return chksum;
    }
};

class NibeGw final : AbstractNibeGw {
   public:
    NibeGw(HardwareSerial* serial, int RS485DirectionPin, int RS485RxPin, int RS485TxPin);

    esp_err_t begin(NibeGwCallback& callback);

    void setAckModbus40Address(boolean val);
    void setAckSms40Address(boolean val);
    void setAckRmu40Address(boolean val);
    void setSendAcknowledge(boolean val);

   private:
    eState state;
    boolean connectionState;
    byte directionPin;
    byte buffer[MAX_DATA_LEN];
    byte index;
    HardwareSerial* RS485;
    int RS485RxPin;
    int RS485TxPin;
    byte verbose;
    boolean ackModbus40;
    boolean ackSms40;
    boolean ackRmu40;
    boolean sendAcknowledge;
    NibeGwCallback* callback;

    int checkNibeMessage(const byte* const data, byte len);
    void sendData(const byte* const data, byte len);
    void sendAck();
    void sendNak();
    boolean shouldAckNakSend(byte address);

    void connect();
    void disconnect();
    boolean connected();
    boolean messageStillOnProgress();

    static void task(void* pvParameters);
    void loop();
};

// for testing
class SimulatedNibeGw final : AbstractNibeGw {
   public:
    esp_err_t begin(NibeGwCallback& callback);

   private:
    NibeGwCallback* callback;

    static void task(void* pvParameters);
    void loop();
};

#endif