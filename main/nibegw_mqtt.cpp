#include "nibegw_mqtt.h"

#include <esp_log.h>

#include <bit>

static const char* TAG = "nibegw_mqtt";

NibeMqttGw::NibeMqttGw() { mqttClient = nullptr; }

esp_err_t NibeMqttGw::begin(const NibeMqttConfig& config, MqttClient& mqttClient) {
    this->config = &config;
    this->mqttClient = &mqttClient;

    readCoilsRingBuffer = xRingbufferCreateNoSplit(sizeof(uint16_t), READ_COILS_RING_BUFFER_SIZE);
    if (readCoilsRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Could not create readCoilsRingBuffer");
        return ESP_FAIL;
    }

    nibeRootTopic = mqttClient.getConfig().rootTopic + "/coils/";
    return ESP_OK;
}

void NibeMqttGw::publishState() {
    // put subscribed coils into queue so that onMessageTokenReceived can send them to nibe
    ESP_LOGI(TAG, "publishState, requesting %d coils", config->coilsToPoll.size());
    for (auto coil : config->coilsToPoll) {
        // TODO: clear ring buffer - nibegw should be fast enough to keep up with the queue
        if (!xRingbufferSend(readCoilsRingBuffer, &coil, sizeof(coil), 0)) {
            ESP_LOGW(TAG, "Could not send coil %d to readCoilsRingBuffer. Buffer full.", coil);
        }
    }
}

void NibeMqttGw::onMessageReceived(const uint8_t* const data, int len) {
    ESP_LOGI(TAG, "onMessageReceived: %s", AbstractNibeGw::dataToString(data, len).c_str());
    NibeResponseMessage* msg = (NibeResponseMessage*)data;
    if (msg->cmd == NIBE_CMD_MODBUS_READ_RESP) {
        uint16_t coilAddress = std::byteswap(msg->readResponse.coilAddress);
        auto iter = config->coils.find(coilAddress);
        if (iter == config->coils.end()) {
            ESP_LOGW(TAG, "Received data for unknown coil %d", coilAddress);
            return;
        }
        const Coil& coil = iter->second;
        // TODO: should check data consistency (len vs data type)
        // decode raw data
        std::string value = coil.decodeCoilData(msg->readResponse);
        // publish data to mqtt
        // TODO: use more descriptive topic (title-coilAddress)?
        mqttClient->publish(nibeRootTopic + std::to_string(coilAddress), value);
    } else {
        ESP_LOGI(TAG, "Unknown message cmd=%x", msg->cmd);
    }
}

int NibeMqttGw::onReadTokenReceived(uint8_t* data) {
    size_t item_size;
    uint16_t* coilAddressPtr = (uint16_t*)xRingbufferReceive(readCoilsRingBuffer, &item_size, 0);
    if (coilAddressPtr == nullptr) {
        return 0;
    }
    uint16_t coilAddress = *coilAddressPtr;
    vRingbufferReturnItem(readCoilsRingBuffer, (void*)coilAddressPtr);

    ESP_LOGI(TAG, "onReadTokenReceived, read coil %d", coilAddress);
    NibeReadRequestMessage* readRequest = (NibeReadRequestMessage*)data;
    readRequest->start = NIBE_REQUEST_START;
    readRequest->cmd = NIBE_CMD_MODBUS_READ_REQ;
    readRequest->coilAddress = std::byteswap((uint16_t)coilAddress);
    readRequest->chksum = AbstractNibeGw::calcCheckSum(data, sizeof(NibeReadRequestMessage) - 1);
    return sizeof(NibeReadRequestMessage);
}

int NibeMqttGw::onWriteTokenReceived(uint8_t* data) {
    // TODO: send data to nibe
    ESP_LOGI(TAG, "onWriteTokenReceived");
    return 0;
}
