#include "nibegw_mqtt.h"

#include <esp_log.h>

#include <bit>

static const char* TAG = "nibegw_mqtt";

NibeMqttGw::NibeMqttGw(Metrics& metrics) : metrics(metrics) { mqttClient = nullptr; }

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

    // TODO: clear ring buffer? - nibegw should be fast enough to keep up with the queue
    for (auto coil : config->coilsToPoll) {
        requestCoil(coil);
    }
}

void NibeMqttGw::requestCoil(uint16_t coilAddress) {
    if (!xRingbufferSend(readCoilsRingBuffer, &coilAddress, sizeof(coilAddress), 0)) {
        ESP_LOGW(TAG, "Could not send coil %d to readCoilsRingBuffer. Buffer full.", coilAddress);
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

        // announce coil on first appearance
        if (announcedCoils.find(coilAddress) == announcedCoils.end()) {
            announceCoil(coil);
        }

        // send coil as metrics
        auto iter2 = coilMetrics.find(coilAddress);
        if (iter2 == coilMetrics.end()) {
            // TODO: configurable name and factor for metrics, name is not prom conform
            Metric& metric = metrics.addMetric(coil.promMetricName().c_str(), coil.factor);
            iter2 = coilMetrics.insert({coilAddress, &metric}).first;
        }
        Metric* metric = iter2->second;
        int32_t valueInt = coil.decodeCoilDataRaw(msg->readResponse);
        metric->setValue(valueInt);
    } else {
        ESP_LOGI(TAG, "Unknown message cmd=%x", msg->cmd);
    }
}

static const char* DISCOVERY_PAYLOAD = R"({
"object_id":"nibegw-coil-%u",
"unique_id":"nibegw-coil-%u",
"name":"%s",
"state_topic":"%s",
%s
%s
})";

void NibeMqttGw::announceCoil(const Coil& coil) {
    ESP_LOGI(TAG, "Announcing coil %u", coil.id);

    const char* component = "sensor";

    char stateTopic[64];
    snprintf(stateTopic, sizeof(stateTopic), "%s%u", nibeRootTopic.c_str(), coil.id);
    char unit[128];
    switch (coil.unit) {
        case CoilUnit::Unknown:
        case CoilUnit::NoUnit:
            unit[0] = '\0';
            break;
        case CoilUnit::GradCelcius:
            snprintf(unit, sizeof(unit),
                     R"("unit_of_measurement":"%s","device_class":"temperature","state_class":"measurement",)",
                     coil.unitAsString());
            break;
        case CoilUnit::Hours:
        case CoilUnit::Minutes:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"duration",)", coil.unitAsString());
            break;
        case CoilUnit::Watt:
        case CoilUnit::KiloWatt:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"power",)", coil.unitAsString());
            break;
        case CoilUnit::WattHour:
        case CoilUnit::KiloWattHour:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"energy",)", coil.unitAsString());
            break;
        case CoilUnit::Hertz:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"frequency",)", coil.unitAsString());
            break;
        default:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s",)", coil.unitAsString());
            break;
    }
    // TODO: writable coils

    // !!! if crash (strlen in ROM) -> stack too small (nibegw.h: NIBE_GW_TASK_STACK_SIZE) or incorrect format string!!!
    char discoveryTopic[64];
    snprintf(discoveryTopic, sizeof(discoveryTopic), "%s/%s/nibegw/coil-%u/config",
             mqttClient->getConfig().discoveryPrefix.c_str(), component, coil.id);
    char discoveryPayload[1024];
    snprintf(discoveryPayload, sizeof(discoveryPayload), DISCOVERY_PAYLOAD, coil.id, coil.id, coil.title.c_str(), stateTopic,
             unit, mqttClient->getDeviceDiscoveryInfo().c_str());

    mqttClient->publish(discoveryTopic, discoveryPayload, QOS0, true);
    announcedCoils.insert(coil.id);
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
