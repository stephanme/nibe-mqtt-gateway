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


class NibeGw final : AbstractNibeGw {
   public:
    NibeGw(HardwareSerial* serial, int RS485DirectionPin, int RS485RxPin, int RS485TxPin);

    esp_err_t begin(NibeGwCallback& callback);

    void setAckModbus40Address(bool val);
    void setAckSms40Address(bool val);
    void setAckRmu40Address(bool val);
    void setSendAcknowledge(bool val);

   private:
    eState state;
    bool connectionState;
    uint8_t directionPin;
    uint8_t buffer[MAX_DATA_LEN];
    uint8_t index;
    HardwareSerial* RS485;
    int RS485RxPin;
    int RS485TxPin;
    uint8_t verbose;
    bool ackModbus40;
    bool ackSms40;
    bool ackRmu40;
    bool sendAcknowledge;
    NibeGwCallback* callback;

    int checkNibeMessage(const uint8_t* const data, uint8_t len);
    void sendData(const uint8_t* const data, uint8_t len);
    void sendAck();
    void sendNak();
    bool shouldAckNakSend(uint8_t address);

    void connect();
    void disconnect();
    bool connected();
    bool messageStillOnProgress();

    static void task(void* pvParameters);
    void loop();
};

#endif