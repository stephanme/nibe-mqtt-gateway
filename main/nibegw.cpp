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

#include "nibegw.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstring>

static const char* TAG = "nibegw";

static char hex[] = "0123456789ABCDEF";

NibeGw::NibeGw(NibeInterface& nibeInterface) : nibeInterface(nibeInterface) {
    state = STATE_WAIT_START;
    index = 0;
}

esp_err_t NibeGw::begin() {
    ESP_LOGI(TAG, "begin");
    int err = xTaskCreatePinnedToCore(&task, "nibegwTask", NIBE_GW_TASK_STACK_SIZE, this, NIBE_GW_TASK_PRIORITY, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start nibegw task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void NibeGw::task(void* pvParameters) {
    NibeGw* gw = (NibeGw*)pvParameters;
    while (1) {
        gw->loop();
        vTaskDelay(1 / portTICK_PERIOD_MS);  // TODO: tuning, loop could read until no more data
    }
}

void NibeGw::loop() {
    switch (state) {
        case STATE_WAIT_START: {
            int b = nibeInterface.readData();
            if (b == (int)NibeStart::Response) {
                buffer[0] = b;
                index = 1;
                state = STATE_WAIT_DATA;

                ESP_LOGV(TAG, "Frame start found");
            }
            break;
        }

        case STATE_WAIT_DATA: {
            int b = nibeInterface.readData();
            if (b >= 0) {
                if (index >= MAX_DATA_LEN) {
                    // too long message
                    state = STATE_WAIT_START;
                } else {
                    buffer[index++] = b;
                    int msglen = checkNibeMessage(buffer, index);
                    ESP_LOGV(TAG, "STATE_WAIT_DATA: %02x, chkMsg=%d", b, msglen);

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
                            state = STATE_OK_MESSAGE_RECEIVED;
                            break;
                    }
                }
            }
            break;
        }

        case STATE_CRC_FAILURE:
            if (shouldAckNakSend(bufferAsMsg->deviceAddress)) sendNak();
            ESP_LOGV(TAG, "STATE_CRC_FAILURE");
            state = STATE_WAIT_START;
            break;

        case STATE_OK_MESSAGE_RECEIVED:
            ESP_LOGV(TAG, "STATE_OK_MESSAGE_RECEIVED");
            // NibeStart::Response is ensured
            // ignore non-modbus messages
            if (bufferAsMsg->deviceAddress == NibeDeviceAddress::MODBUS40) {
                NibeGwCallback* callback = this->callback;
                if (bufferAsMsg->cmd == NibeCmd::ModbusReadReq && bufferAsMsg->len == 0) {
                    ESP_LOGV(TAG, "READ_TOKEN received");
                    int msglen = callback != nullptr ? callback->onReadTokenReceived((NibeReadRequestMessage*)buffer) : 0;
                    sendResponseMessage(msglen);
                } else if (bufferAsMsg->cmd == NibeCmd::ModbusWriteReq && bufferAsMsg->len == 0) {
                    ESP_LOGV(TAG, "WRITE_TOKEN received");
                    int msglen = callback != nullptr ? callback->onWriteTokenReceived((NibeWriteRequestMessage*)buffer) : 0;
                    sendResponseMessage(msglen);
                } else {
                    sendAck();
                    ESP_LOGV(TAG, "Message received");
                    if (callback != nullptr) callback->onMessageReceived(bufferAsMsg, index);
                }
            }
            state = STATE_WAIT_START;
            break;
    }
}

void NibeGw::sendResponseMessage(int len) {
    if (len > 0) {
        nibeInterface.sendData(buffer, len);
    } else {
        if (shouldAckNakSend(bufferAsMsg->deviceAddress)) sendAck();
    }
}

/*
   Return:
    >0 if valid message received (return message len)
     0 if ok, but message not ready
    -1 if invalid message
    -2 if checksum fails
*/
int NibeGw::checkNibeMessage(const uint8_t* const data, uint8_t len) {
    if (len <= 0) return 0;

    if (len >= 1) {
        NibeResponseMessage* msg = (NibeResponseMessage*)data;
        if (msg->start != NibeStart::Response) return -1;

        if (len >= 6) {
            int datalen = msg->len;
            if (len < datalen + 6) return 0;

            uint8_t checksum = calcCheckSum(data + 1, datalen + 4);
            uint8_t msg_checksum = data[datalen + 5];
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

void NibeGw::sendAck() {
    nibeInterface.sendData(0x06);
    ESP_LOGV(TAG, "Send ACK");
}

void NibeGw::sendNak() {
    nibeInterface.sendData(0x15);
    ESP_LOGV(TAG, "Send NAK");
}

bool NibeGw::shouldAckNakSend(NibeDeviceAddress address) { return address == NibeDeviceAddress::MODBUS40; }

std::string NibeGw::dataToString(const uint8_t* const data, int len) {
    std::string s;
    s.reserve(len * 3);
    for (int i = 0; i < len; i++) {
        s += hex[data[i] >> 4];
        s += hex[data[i] & 0xf];
        s += " ";
    }
    return s;
}
