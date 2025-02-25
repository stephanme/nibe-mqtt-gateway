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

#ifndef _nibegw_rs485_h_
#define _nibegw_rs485_h_

#include <Arduino.h>

#include "nibegw.h"

class NibeRS485 final : public NibeInterface {
   public:
    NibeRS485(HardwareSerial* serial, int RS485DirectionPin, int RS485RxPin, int RS485TxPin);

    esp_err_t begin();

    // NibeInterface
    virtual boolean isDataAvailable();
    virtual int readData();
    virtual void sendData(const uint8_t* const data, uint8_t len);
    virtual void sendData(const uint8_t data);

   private:
    bool connectionState;
    uint8_t directionPin;
    HardwareSerial* RS485;
    int RS485RxPin;
    int RS485TxPin;

    void connect();
    void disconnect();

    // buffer for logging received data (all, also for other devices than modbus 40)
    // mqtt logging: LOG_ITEM_SIZE=256
    // 1 byte = 3 chars
    // static text = V (17:47:58.777) nibegw: Rec: = 29 chars + 1 \0
    // (256 - 30) / 3 ~ 75 -> 64 bytes
    // 9600 baud = 960 bytes/s -> max 15 logs/s
    uint8_t readLogBuffer[64];
    int readLogIndex;
};

#endif