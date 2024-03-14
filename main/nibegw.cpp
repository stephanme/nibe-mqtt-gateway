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

static const char* TAG = "nibegw";

static char hex[] = "0123456789ABCDEF";

std::string AbstractNibeGw::dataToString(const uint8_t* const data, int len) {
    std::string s;
    s.reserve(len * 3);
    for (int i = 0; i < len; i++) {
        s += hex[data[i] >> 4];
        s += hex[data[i] & 0xf];
        s += " ";
    }
    return s;
}

esp_err_t SimulatedNibeGw::begin(NibeGwCallback& callback) {
    ESP_LOGI(TAG, "SimulatedNibeGw::begin");
    this->callback = &callback;
    int err = xTaskCreatePinnedToCore(&task, "nibegwTask", NIBE_GW_TASK_STACK_SIZE, this, NIBE_GW_TASK_PRIORITY, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start nibegw task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void SimulatedNibeGw::task(void* pvParameters) {
    SimulatedNibeGw* gw = (SimulatedNibeGw*)pvParameters;
    while (1) {
        gw->loop();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void SimulatedNibeGw::loop() {
    // read token
    NibeReadRequestMessage request;
    int len = callback->onReadTokenReceived(&request);

    if (len > 0) {
        // simulate response
        uint8_t responseMsg[MAX_DATA_LEN];
        NibeResponseMessage* response = (NibeResponseMessage*)responseMsg;
        response->start = NibeStart::Response;
        response->deviceAddress = NibeDeviceAddress::MODBUS40;
        response->cmd = NibeCmd::ModbusReadResp;
        response->len = sizeof(NibeReadResponseData);
        response->readResponse.coilAddress = request.coilAddress;  // is already swapped
        response->readResponse.value[0] = 0x01;
        response->readResponse.value[1] = 0x02;
        response->readResponse.value[2] = 0x03;
        response->readResponse.value[3] = 0x04;
        response->data[response->len] = calcCheckSum(responseMsg + 1, response->len + 4);  // address..data
        callback->onMessageReceived(response, sizeof(NibeResponseMessage) + 1);
    }
}
