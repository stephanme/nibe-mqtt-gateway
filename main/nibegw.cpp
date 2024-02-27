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

#include <bit>

static const char* TAG = "nibegw";

esp_err_t SimulatedNibeGw::begin(NibeGwCallback& callback) {
    ESP_LOGI(TAG, "SimulatedNibeGw::begin");
    this->callback = &callback;
    int err = xTaskCreatePinnedToCore(&task, "nibegwTask", 6 * 1024, this, NIBE_GW_TASK_PRIORITY, NULL, 1);
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
    uint8_t requestMsg[MAX_DATA_LEN];
    int len = callback->onReadTokenReceived(requestMsg);

    if (len > 0) {
        NibeReadRequestMessage* request = (NibeReadRequestMessage*)requestMsg;

        // simulate response
        uint8_t responseMsg[MAX_DATA_LEN];
        NibeResponseMessage* response = (NibeResponseMessage*)responseMsg;
        response->start = NIBE_RESPONSE_START;
        response->address = std::byteswap((uint16_t)NIBE_ADDRESS_MODBUS);
        response->cmd = NIBE_CMD_MODBUS_READ_RESP;
        response->len = sizeof(NibeReadResponseData);
        response->readResponse.coilAddress = request->coilAddress;  // is already swapped
        response->readResponse.value[0] = 0x01;
        response->readResponse.value[1] = 0x02;
        response->readResponse.value[2] = 0x03;
        response->readResponse.value[3] = 0x04;
        response->data[response->len] = calcCheckSum(responseMsg + 1, response->len + 4);  // address..data
        callback->onMessageReceived(responseMsg, sizeof(NibeResponseMessage) + 1);
    }
}
