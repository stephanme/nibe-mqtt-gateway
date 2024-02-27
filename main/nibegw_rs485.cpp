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
 * Author: pauli.anttila@gmail.com
 *
 */

// see nibegw.h for changes

#include "nibegw_rs485.h"

#include <esp_log.h>
#include <freertos/task.h>
#include <bit>

static const char* TAG = "nibegw";

NibeGw::NibeGw(HardwareSerial* serial, int RS485DirectionPin, int RS485RxPin, int RS485TxPin) {
    verbose = 0;
    ackModbus40 = true;
    ackSms40 = false;
    ackRmu40 = false;
    sendAcknowledge = true;
    state = STATE_WAIT_START;
    connectionState = false;
    RS485 = serial;
    directionPin = RS485DirectionPin;
    this->RS485RxPin = RS485RxPin;
    this->RS485TxPin = RS485TxPin;
    pinMode(directionPin, OUTPUT);
    digitalWrite(directionPin, LOW);
}

esp_err_t NibeGw::begin(NibeGwCallback& callback) {
    ESP_LOGI(TAG, "NibeGw::begin");
    this->callback = &callback;
    int err = xTaskCreatePinnedToCore(&task, "nibegwTask", 6 * 1024, this, NIBE_GW_TASK_PRIORITY, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start nibegw task");
        return ESP_FAIL;
    }

    connect();
    return ESP_OK;
}

void NibeGw::task(void* pvParameters) {
    NibeGw* gw = (NibeGw*)pvParameters;
    while (1) {
        gw->loop();
        delay(1);  // TODO: tuning, loop could read until no more data
    }
}

void NibeGw::connect() {
    if (!connectionState) {
        state = STATE_WAIT_START;

        RS485->begin(9600, SERIAL_8N1, RS485RxPin, RS485TxPin);

        connectionState = true;
    }
}

void NibeGw::disconnect() {
    if (connectionState) {
        RS485->end();
        connectionState = false;
    }
}

boolean NibeGw::connected() { return connectionState; }

void NibeGw::setAckModbus40Address(boolean val) { ackModbus40 = val; }

void NibeGw::setAckSms40Address(boolean val) { ackSms40 = val; }

void NibeGw::setAckRmu40Address(boolean val) { ackRmu40 = val; }

void NibeGw::setSendAcknowledge(boolean val) { sendAcknowledge = val; }

boolean NibeGw::messageStillOnProgress() {
    if (!connectionState) return false;

    if (RS485->available() > 0) return true;

    if (state == STATE_CRC_FAILURE || state == STATE_OK_MESSAGE_RECEIVED) return true;

    return false;
}

void NibeGw::loop() {
    if (!connectionState) return;

    switch (state) {
        case STATE_WAIT_START:
            if (RS485->available() > 0) {
                byte b = RS485->read();

                if (b == 0x5C) {
                    buffer[0] = b;
                    index = 1;
                    state = STATE_WAIT_DATA;

                    ESP_LOGV(TAG, "Frame start found");
                }
            }
            break;

        case STATE_WAIT_DATA:
            if (RS485->available() > 0) {
                byte b = RS485->read();

                if (index >= MAX_DATA_LEN) {
                    // too long message
                    state = STATE_WAIT_START;
                } else {
                    buffer[index++] = b;
                    int msglen = checkNibeMessage(buffer, index);

                    switch (msglen) {
                        case 0:
                            break;  // Ok, but not ready
                        case -1:
                            state = STATE_WAIT_START;
                            break;  // Invalid message
                        case -2:
                            state = STATE_CRC_FAILURE;
                            break;  // Checksum error
                        default:
                            ESP_LOGV(TAG, "checkMsg=%d", msglen);
                            state = STATE_OK_MESSAGE_RECEIVED;
                            break;
                    }
                }
            }
            break;

        case STATE_CRC_FAILURE:
            if (shouldAckNakSend(buffer[2])) sendNak();
            ESP_LOGV(TAG, "CRC failure");
            state = STATE_WAIT_START;
            break;

        case STATE_OK_MESSAGE_RECEIVED:
            if (buffer[0] == 0x5C && buffer[1] == 0x00 && buffer[2] == 0x20 && buffer[4] == 0x00 &&
                (buffer[3] == 0x69 || buffer[3] == 0x6B)) {
                
                int msglen;
                if (buffer[3] == 0x6B) {
                    ESP_LOGV(TAG, "WRITE_TOKEN received");
                    msglen = callback->onWriteTokenReceived(buffer);
                } else {
                    ESP_LOGV(TAG, "READ_TOKEN received");
                    msglen = callback->onReadTokenReceived(buffer);
                }
                if (msglen > 0) {
                    sendData(buffer, (byte)msglen);
                } else {
                    if (shouldAckNakSend(buffer[2])) sendAck();
                    ESP_LOGV(TAG, "No message to send");
                }
            } else {
                if (shouldAckNakSend(buffer[2])) sendAck();
                ESP_LOGV(TAG, "Message received");
                callback->onMessageReceived(buffer, index);
            }
            state = STATE_WAIT_START;
            break;
    }
}

/*
   Return:
    >0 if valid message received (return message len)
     0 if ok, but message not ready
    -1 if invalid message
    -2 if checksum fails
*/
int NibeGw::checkNibeMessage(const byte* const data, byte len) {
    if (len <= 0) return 0;

    if (len >= 1) {
        if (data[0] != 0x5C) return -1;

        if (len >= 6) {
            int datalen = data[4];

            if (len < datalen + 6) return 0;

            byte checksum = 0;

            // calculate XOR checksum
            for (int i = 1; i < (datalen + 5); i++) checksum ^= data[i];

            byte msg_checksum = data[datalen + 5];
            ESP_LOGV(TAG, "checksum=%02X, msg_checksum=%02X", checksum, msg_checksum);

            if (checksum != msg_checksum) {
                // if checksum is 0x5C (start character),
                // heat pump seems to send 0xC5 checksum
                if (checksum != 0x5C && msg_checksum != 0xC5) return -2;
            }

            return datalen + 6;
        }
    }

    return 0;
}

void NibeGw::sendData(const byte* const data, byte len) {
    digitalWrite(directionPin, HIGH);
    delay(1);
    RS485->write(data, len);
    RS485->flush();
    delay(1);
    digitalWrite(directionPin, LOW);

    ESP_LOGV(TAG, "Send message to heat pump: len=%d", len);
}

void NibeGw::sendAck() {
    digitalWrite(directionPin, HIGH);
    delay(1);
    RS485->write(0x06);
    RS485->flush();
    delay(1);
    digitalWrite(directionPin, LOW);
    ESP_LOGV(TAG, "Send ACK");
}

void NibeGw::sendNak() {
    digitalWrite(directionPin, HIGH);
    delay(1);
    RS485->write(0x15);
    RS485->flush();
    delay(1);
    digitalWrite(directionPin, LOW);
    ESP_LOGV(TAG, "Send NAK");
}

boolean NibeGw::shouldAckNakSend(byte address) {
    if (sendAcknowledge) {
        if (address == MODBUS40 && ackModbus40)
            return true;
        else if (address == RMU40 && ackRmu40)
            return true;
        else if (address == SMS40 && ackSms40)
            return true;
    }

    return false;
}
