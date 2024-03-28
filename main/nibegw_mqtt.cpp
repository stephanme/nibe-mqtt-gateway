#include "nibegw_mqtt.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstring>

static const char* TAG = "nibegw_mqtt";

NibeMqttGw::NibeMqttGw(Metrics& metrics)
    : metrics(metrics), metricPublishStateTime(metrics.addMetric(R"(nibegw_task_runtime_seconds{task="publishCoils"})", 1000)) {
    mqttClient = nullptr;
}

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
    lastPublishStateStartTime = esp_timer_get_time() / 1000;
    for (auto coil : config->coilsToPoll) {
        requestCoil(coil);
    }
}

void NibeMqttGw::requestCoil(uint16_t coilAddress) {
    if (!xRingbufferSend(readCoilsRingBuffer, &coilAddress, sizeof(coilAddress), 0)) {
        ESP_LOGW(TAG, "Could not send coil %d to readCoilsRingBuffer. Buffer full.", coilAddress);
    }
}

void NibeMqttGw::onMessageReceived(const NibeResponseMessage* const msg, int len) {
    switch (msg->cmd) {
        case NibeCmd::ModbusReadResp: {
            ESP_LOGV(TAG, "onMessageReceived ModbusReadResp: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());
            const Coil* coil = findCoil(msg->readResponse.coilAddress);
            if (coil == nullptr) {
                ESP_LOGW(TAG, "Received NibeResponseMessage for unknown coil %d", msg->readResponse.coilAddress);
                return;
            }

            publishMqtt(*coil, msg->readResponse.value);
            publishMetric(*coil, msg->readResponse.value);
            break;
        }

        case NibeCmd::ModbusDataMsg: {
            // ~ one ModMusDataMsg ever 2s, up to 20 coils
            // Limitation: can only handle 16bit coils
            ESP_LOGV(TAG, "onMessageReceived ModbusDataMsg: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());

            int receivedCoils = 0;
            int publishIdx = modbusDataMsgMqttPublish % 20;
            for (int i = 0; i < 20; i++) {
                NibeDataMessageCoil coilData = msg->dataMessage.coils[i];
                if (coilData.coilAddress == 0xFFFF) {
                    // 0xffff indicates that not all 20 coils in ModbusDataMsg are used
                    break;
                }
                const Coil* coil = findCoil(coilData.coilAddress);
                if (coil == nullptr) {
                    ESP_LOGW(TAG, "Received ModbusDataMsg for unknown coil %d", coilData.coilAddress);
                    continue;
                }
                switch (coil->dataType) {
                    case CoilDataType::UInt8:
                    case CoilDataType::Int8:
                    case CoilDataType::UInt16:
                    case CoilDataType::Int16:
                        break;
                    default:
                        ESP_LOGW(TAG, "Unsupported data type %d in ModbusDataMsg for coil %d", (int)coil->dataType, coil->id);
                        continue;
                }

                // publish 2 registers every ModbusDataMsg = every 2s
                // -> ~1 coil/s or all 20 coils take ~20s which is around the same speed as polling
                if (i == publishIdx || i == publishIdx + 1) {
                    publishMqtt(*coil, coilData.value);
                }
                publishMetric(*coil, coilData.value);
                receivedCoils++;
            }
            modbusDataMsgMqttPublish += 2;  // 2 coils published per ModbusDataMsg
            ESP_LOGD(TAG, "onMessageReceived ModbusDataMsg: received %d coils", receivedCoils);
            break;
        }

        case NibeCmd::ProductInfoMsg:
        case NibeCmd::AccessoryVersionReq:
            // known but ignored commands
            ESP_LOGV(TAG, "onMessageReceived: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());
            break;

        default:
            ESP_LOGI(TAG, "onMessageReceived UNKNOWN cmd %d: %s", (int)msg->cmd,
                     NibeGw::dataToString((uint8_t*)msg, len).c_str());
            break;
    }
}

const Coil* NibeMqttGw::findCoil(uint16_t coilAddress) {
    auto iter = config->coils.find(coilAddress);
    if (iter == config->coils.end()) {
        ESP_LOGW(TAG, "Received data for unknown coil %d", coilAddress);
        return nullptr;
    }
    return &iter->second;
}

// send coils as mqtt messages, announce new coils for HA auto-discovery
void NibeMqttGw::publishMqtt(const Coil& coil, const uint8_t* const data) {
    // TODO: should check data consistency (len vs data type)
    // decode raw data
    std::string value = coil.decodeCoilData(data);
    // publish data to mqtt
    // TODO: use more descriptive topic (title-coilAddress)?
    mqttClient->publish(nibeRootTopic + std::to_string(coil.id), value);

    // announce coil on first appearance
    if (announcedCoils.find(coil.id) == announcedCoils.end()) {
        announceCoil(coil);
    }
}

// send coil as metrics, create metric if not exists but only for coils that are configured as metrics
void NibeMqttGw::publishMetric(const Coil& coil, const uint8_t* const data) {
    auto iter2 = coilMetrics.find(coil.id);
    if (iter2 == coilMetrics.end()) {
        const NibeCoilMetricConfig& metricCfg = coil.toPromMetricConfig(*config);
        if (metricCfg.isValid()) {
            Metric& metric = metrics.addMetric(metricCfg.name.c_str(), metricCfg.factor, metricCfg.scale);
            iter2 = coilMetrics.insert({coil.id, &metric}).first;
        } else {
            // do not publish this coil as metric
            iter2 = coilMetrics.insert({coil.id, nullptr}).first;
        }
    }
    Metric* metric = iter2->second;
    if (metric != nullptr) {
        int32_t valueInt = coil.decodeCoilDataRaw(data);
        metric->setValue(valueInt);
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
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"duration","state_class":"total",)",
                     coil.unitAsString());
            break;
        case CoilUnit::Minutes:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"duration","state_class":"measurement",)",
                     coil.unitAsString());
            break;
        case CoilUnit::Watt:
        case CoilUnit::KiloWatt:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"power","state_class":"measurement",)",
                     coil.unitAsString());
            break;
        case CoilUnit::WattHour:
        case CoilUnit::KiloWattHour:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"energy","state_class":"total",)",
                     coil.unitAsString());
            break;
        case CoilUnit::Hertz:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","device_class":"frequency","state_class":"measurement",)",
                     coil.unitAsString());
            break;
        default:
            snprintf(unit, sizeof(unit), R"("unit_of_measurement":"%s","state_class":"measurement",)", coil.unitAsString());
            break;
    }

    // special handling for certain coils
    switch (coil.id) {
        case 40940:
        case 43005:  // degree minutes, no unit
            std::strcpy(unit, R"("state_class":"measurement",)");
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

int NibeMqttGw::onReadTokenReceived(NibeReadRequestMessage* readRequest) {
    size_t item_size;
    uint16_t* coilAddressPtr = (uint16_t*)xRingbufferReceive(readCoilsRingBuffer, &item_size, 0);
    if (coilAddressPtr == nullptr) {
        // no more coils to read
        // calculate time to publish state
        uint32_t startTime = lastPublishStateStartTime.exchange(0);
        if (startTime > 0) {
            uint32_t now = esp_timer_get_time() / 1000;
            metricPublishStateTime.setValue(now - startTime);
        }
        return 0;
    }
    uint16_t coilAddress = *coilAddressPtr;
    vRingbufferReturnItem(readCoilsRingBuffer, (void*)coilAddressPtr);

    readRequest->start = NibeStart::Request;
    readRequest->cmd = NibeCmd::ModbusReadReq;
    readRequest->len = 2;
    readRequest->coilAddress = coilAddress;
    readRequest->chksum = NibeGw::calcCheckSum((uint8_t*)readRequest, sizeof(NibeReadRequestMessage) - 1);

    ESP_LOGD(TAG, "onReadTokenReceived, read coil %d: %s", (int)coilAddress,
             NibeGw::dataToString((uint8_t*)readRequest, sizeof(NibeReadRequestMessage)).c_str());
    return sizeof(NibeReadRequestMessage);
}

int NibeMqttGw::onWriteTokenReceived(NibeWriteRequestMessage* data) {
    // TODO: send data to nibe
    // ESP_LOGD(TAG, "onWriteTokenReceived");
    return 0;
}
