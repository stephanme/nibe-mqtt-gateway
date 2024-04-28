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
    writeCoilsRingBuffer = xRingbufferCreateNoSplit(sizeof(NibeMqttGwWriteRequest), WRITE_COILS_RING_BUFFER_SIZE);
    if (readCoilsRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Could not create writeCoilsRingBuffer");
        return ESP_FAIL;
    }

    nibeRootTopic = mqttClient.getConfig().rootTopic + "/coils/";

    // subscribe to 'set' topic of all coils
    std::string commandTopic = nibeRootTopic + "+/set";
    mqttClient.subscribe(commandTopic, this);

    // pre-announce known coils for HA auto-discovery, offloads nibegw task
    // polled coilds
    for (const auto& coilId : config.coilsToPoll) {
        auto iter = config.coils.find(coilId);
        if (iter != config.coils.end()) {
            announceCoil(iter->second);
        }
    }
    // nibegw doesn't know upfront about coils sent as NibeDataMessage (20 fast coils) -> get announced on first data

    // announce all coils with homeassistantDiscoveryOverrides as overrides are expensive (json parsing)
    // writable coils need to be pre-announced and therefore always require overrides (even if empty)
    for (auto ovrIter = config.homeassistantDiscoveryOverrides.cbegin(); ovrIter != config.homeassistantDiscoveryOverrides.cend();
         ovrIter++) {
        auto iter = config.coils.find(ovrIter->first);
        if (iter != config.coils.end()) {
            announceCoil(iter->second);
        }
    }
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

// topic: nibegw/coils/<id>/set
// payload: new value
void NibeMqttGw::onMqttMessage(const std::string& topic, const std::string& payload) {
    ESP_LOGI(TAG, "Received MQTT message: %s: %s", topic.c_str(), payload.c_str());
    if (!topic.starts_with(nibeRootTopic) || !topic.ends_with("/set")) {
        ESP_LOGW(TAG, "Invalid topic %s", topic.c_str());
        return;
    }
    uint16_t coilId = strtoul(topic.c_str() + nibeRootTopic.length(), nullptr, 10);
    if (coilId == 0) {
        ESP_LOGW(TAG, "Invalid topic %s", topic.c_str());
        return;
    }
    writeCoil(coilId, payload.c_str());
}

void NibeMqttGw::requestCoil(uint16_t coilAddress) {
    if (!xRingbufferSend(readCoilsRingBuffer, &coilAddress, sizeof(coilAddress), 0)) {
        ESP_LOGW(TAG, "Could not send coil %d to readCoilsRingBuffer. Buffer full.", coilAddress);
    }
}

void NibeMqttGw::writeCoil(uint16_t coilAddress, const char* value) {
    if (value == nullptr) {
        ESP_LOGE(TAG, "writeCoil: value is null for coil %d", coilAddress);
        return;
    }
    auto valueLen = strlen(value);
    if (valueLen == 0) {
        ESP_LOGE(TAG, "writeCoil: missing value for coil %d", coilAddress);
        return;
    }
    if (valueLen > sizeof(NibeMqttGwWriteRequest::value) - 1) {
        ESP_LOGE(TAG, "writeCoil: value too long for coil %d: %s", coilAddress, value);
        return;
    }
    NibeMqttGwWriteRequest writeRequest;
    writeRequest.coilAddress = coilAddress;
    strncpy(writeRequest.value, value, sizeof(writeRequest.value));
    writeRequest.value[sizeof(writeRequest.value) - 1] = '\0';  // ensure null termination
    if (!xRingbufferSend(writeCoilsRingBuffer, &writeRequest, sizeof(writeRequest), 0)) {
        ESP_LOGE(TAG, "Could not send coil %d to writeCoilsRingBuffer. Buffer full.", coilAddress);
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

        case NibeCmd::ModbusWriteResp: {
            ESP_LOGV(TAG, "onMessageReceived ModbusWriteResp: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());
            // TODO: would be nice to know which coil was written
            if (msg->writeResponse.result == 0) {
                ESP_LOGW(TAG, "Last ModbusWriteReq failed");
            } else {
                ESP_LOGD(TAG, "Last ModbusWriteReq succeeded");
            }
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
            Metric& metric = metrics.addMetric(metricCfg.name.c_str(), metricCfg.factor, metricCfg.scale, metricCfg.counter);
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

void NibeMqttGw::announceCoil(const Coil& coil) {
    ESP_LOGI(TAG, "Announcing coil %u", coil.id);

    auto discoveryDoc = coil.homeassistantDiscoveryMessage(*config, nibeRootTopic, mqttClient->getDeviceDiscoveryInfoRef());
    // !!! if crash (strlen in ROM) -> stack too small (nibegw.h: NIBE_GW_TASK_STACK_SIZE) or incorrect format string!!!
    char discoveryTopic[64];
    const char* component = discoveryDoc["_component_"] | "sensor";
    snprintf(discoveryTopic, sizeof(discoveryTopic), "%s/%s/nibegw/coil-%u/config",
             mqttClient->getConfig().discoveryPrefix.c_str(), component, coil.id);

    discoveryDoc.remove("_component_");
    std::string discoveryMsg;
    serializeJson(discoveryDoc, discoveryMsg);
    mqttClient->publish(discoveryTopic, discoveryMsg, QOS0, true);
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

int NibeMqttGw::onWriteTokenReceived(NibeWriteRequestMessage* writeRequest) {
    size_t item_size;
    NibeMqttGwWriteRequest* coilAddressPtr = (NibeMqttGwWriteRequest*)xRingbufferReceive(writeCoilsRingBuffer, &item_size, 0);
    if (coilAddressPtr == nullptr) {
        // no more coils to write
        return 0;
    }
    uint16_t coilAddress = coilAddressPtr->coilAddress;
    const Coil* coil = findCoil(coilAddress);
    if (coil == nullptr) {
        vRingbufferReturnItem(writeCoilsRingBuffer, (void*)coilAddressPtr);
        ESP_LOGW(TAG, "Received write request for unknown coil %d", coilAddress);
        return 0;
    }
    if (coil->mode == CoilMode::Read) {
        vRingbufferReturnItem(writeCoilsRingBuffer, (void*)coilAddressPtr);
        ESP_LOGW(TAG, "Received write request for read-only coil %d", coilAddress);
        return 0;
    }

    writeRequest->start = NibeStart::Request;
    writeRequest->cmd = NibeCmd::ModbusWriteReq;
    writeRequest->len = 6;
    writeRequest->coilAddress = coilAddress;
    if (!coil->encodeCoilData(coilAddressPtr->value, writeRequest->value)) {
        vRingbufferReturnItem(writeCoilsRingBuffer, (void*)coilAddressPtr);
        return 0;
    }
    writeRequest->chksum = NibeGw::calcCheckSum((uint8_t*)writeRequest, sizeof(NibeWriteRequestMessage) - 1);

    vRingbufferReturnItem(writeCoilsRingBuffer, (void*)coilAddressPtr);

    ESP_LOGD(TAG, "onWriteTokenReceived for coil %d: %s", (int)coilAddress,
             NibeGw::dataToString((uint8_t*)writeRequest, sizeof(NibeWriteRequestMessage)).c_str());
    return sizeof(NibeWriteRequestMessage);
}
