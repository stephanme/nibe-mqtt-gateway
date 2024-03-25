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
    callback = nullptr;
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
        gw->stateMachineLoop();
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// processes all data available from NibeInterface
void NibeGw::stateMachineLoop() {
    while (1) {
        int b = nibeInterface.readData();
        if (b < 0) {
            // no more input -> return
            // ESP_LOGV(TAG, "state=%d, read=-1", state);
            return;
        }
        // enable only for tests or protocol debugging on linux target, overloads ESP32 when connected to heat pump
        // ESP_LOGV(TAG, "state=%d, read=%02X", state, b);
        switch (state) {
            case STATE_WAIT_START: {
                if (b == (int)NibeStart::Response) {
                    bufferAsMsg->start = NibeStart::Response;
                    state = STATE_WAIT_MODBUS40_1;
                    checksum = 0;

                    ESP_LOGV(TAG, "Frame start found");
                }
                break;
            }

            case STATE_WAIT_MODBUS40_1: {
                if (b == ((int)NibeDeviceAddress::MODBUS40 & 0xff)) {
                    checksum ^= b;
                    state = STATE_WAIT_MODBUS40_2;
                } else {
                    state = STATE_WAIT_START;
                }
                break;
            }

            case STATE_WAIT_MODBUS40_2: {
                if (b == ((int)NibeDeviceAddress::MODBUS40 >> 8)) {
                    bufferAsMsg->deviceAddress = NibeDeviceAddress::MODBUS40;
                    checksum ^= b;
                    state = STATE_WAIT_CMD;
                } else {
                    state = STATE_WAIT_START;
                }
                break;
            }

            case STATE_WAIT_CMD: {
                bufferAsMsg->cmd = (NibeCmd)b;
                checksum ^= b;
                state = STATE_WAIT_LEN;
                break;
            }

            case STATE_WAIT_LEN: {
                bufferAsMsg->len = b;
                checksum ^= b;
                index = 5;
                if (index < bufferAsMsg->len + 5) {
                    state = STATE_WAIT_DATA;
                } else {
                    state = STATE_WAIT_CRC;
                }
                break;
            }

            case STATE_WAIT_DATA: {
                if (index >= MAX_DATA_LEN - 1) {
                    // too long message, keep 1 char for CRC
                    state = STATE_WAIT_START;
                } else {
                    buffer[index++] = b;
                    checksum ^= b;
                    if (index >= bufferAsMsg->len + 5) {
                        state = STATE_WAIT_CRC;
                    } else if (b == (int)NibeStart::Response) {
                        // check for duplicated start character 5C
                        state = STATE_WAIT_DATA_5C;
                    }
                }
                break;
            }

            case STATE_WAIT_DATA_5C: {
                if (b == (int)NibeStart::Response) {
                    // duplicated start character in data, skip one
                    bufferAsMsg->len--;
                } else {
                    // should not happen
                    // TODO: handle too long msg
                    buffer[index++] = b;
                }
                checksum ^= b;
                if (index >= bufferAsMsg->len + 5) {
                    state = STATE_WAIT_CRC;
                } else {
                    state = STATE_WAIT_DATA;
                }
                break;
            }

            case STATE_WAIT_CRC: {
                buffer[index++] = b;
                ESP_LOGV(TAG, "checksum=%02X, msg_checksum=%02X", checksum, b);

                if (checksum == b || (checksum == 0x5C && b == 0xC5)) {
                    // if checksum is 0x5C (start character), heat pump seems to send 0xC5 checksum
                    processReceivedModbusMessage();
                } else {
                    sendNak();
                    ESP_LOGV(TAG, "Checksum failure");
                }
                state = STATE_WAIT_START;
                break;
            }
        }
    }
}

void NibeGw::processReceivedModbusMessage() {
    // NibeStart::Response and NibeDeviceAddress::MODBUS40 are ensured by state machine
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
        ESP_LOGV(TAG, "Message received, cmd=%02X", (uint8_t)bufferAsMsg->cmd);
        if (callback != nullptr) callback->onMessageReceived(bufferAsMsg, index);
    }
}

void NibeGw::sendResponseMessage(int len) {
    if (len > 0) {
        nibeInterface.sendData(buffer, len);
    } else {
        // is only called when processing a modbus message
        sendAck();
    }
}

void NibeGw::sendAck() {
    nibeInterface.sendData(0x06);
    ESP_LOGV(TAG, "Send ACK");
}

void NibeGw::sendNak() {
    nibeInterface.sendData(0x15);
    ESP_LOGV(TAG, "Send NAK");
}

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
