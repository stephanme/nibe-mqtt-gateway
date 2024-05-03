#include "nibegw_mqtt.h"

#include <esp_log.h>
#include <esp_timer.h>

#include <cstring>

static const char* TAG = "nibegw_mqtt";

NibeMqttGw::NibeMqttGw(Metrics& metrics)
    : metrics(metrics), metricPublishStateTime(metrics.addMetric(R"(nibegw_task_runtime_seconds{task="publishNibeRegisters"})", 1000)) {
    mqttClient = nullptr;
    numNibeRegistersToPoll = 0;
}

esp_err_t NibeMqttGw::begin(const NibeMqttConfig& config, MqttClient& mqttClient) {
    this->config = &config;
    this->mqttClient = &mqttClient;

    readNibeRegistersRingBuffer = xRingbufferCreateNoSplit(sizeof(uint16_t), READ_REGISTER_RING_BUFFER_SIZE);
    if (readNibeRegistersRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Could not create readNibeRegistersRingBuffer");
        return ESP_FAIL;
    }
    writeNibeRegistersRingBuffer = xRingbufferCreateNoSplit(sizeof(NibeMqttGwWriteRequest), WRITE_REGISTER_RING_BUFFER_SIZE);
    if (readNibeRegistersRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Could not create writeNibeRegistersRingBuffer");
        return ESP_FAIL;
    }

    nibeRootTopic = mqttClient.getConfig().rootTopic + "/coils/";

    // subscribe to 'set' topic of all registers
    std::string commandTopic = nibeRootTopic + "+/set";
    mqttClient.subscribe(commandTopic, this);

    nextNibeRegisterToPollSlow = config.pollRegistersSlow.cbegin();
    numNibeRegistersToPoll = config.pollRegisters.size() + (config.pollRegistersSlow.size() > 0 ? 1 : 0);

    // pre-announce known registers for HA auto-discovery, offloads nibegw task
    // polled registers
    for (const auto& regAdr : config.pollRegisters) {
        auto iter = config.registers.find(regAdr);
        if (iter != config.registers.end()) {
            announceNibeRegister(iter->second);
        }
    }
    for (const auto& regAdr : config.pollRegistersSlow) {
        auto iter = config.registers.find(regAdr);
        if (iter != config.registers.end()) {
            announceNibeRegister(iter->second);
        }
    }
    // nibegw doesn't know upfront about registers sent as NibeDataMessage (20 fast registers) -> get announced on first data

    // announce all registers with homeassistantDiscoveryOverrides as overrides are expensive (json parsing)
    // writable registers need to be pre-announced and therefore always require overrides (even if empty)
    for (auto ovrIter = config.homeassistantDiscoveryOverrides.cbegin(); ovrIter != config.homeassistantDiscoveryOverrides.cend();
         ovrIter++) {
        auto iter = config.registers.find(ovrIter->first);
        if (iter != config.registers.end()) {
            announceNibeRegister(iter->second);
        }
    }
    return ESP_OK;
}

void NibeMqttGw::publishState() {
    // put subscribed registers into queue so that onMessageTokenReceived can send them to nibe
    ESP_LOGI(TAG, "publishState, requesting %d registers", numNibeRegistersToPoll);

    // TODO: clear ring buffer? - nibegw should be fast enough to keep up with the queue
    lastPublishStateStartTime = esp_timer_get_time() / 1000;
    for (auto address : config->pollRegisters) {
        requestNibeRegister(address);
    }
    // plus one register from low frequency list
    if (nextNibeRegisterToPollSlow  == config->pollRegistersSlow.cend()) {
        nextNibeRegisterToPollSlow = config->pollRegistersSlow.cbegin();
    }
    if (nextNibeRegisterToPollSlow != config->pollRegistersSlow.cend()) {
        requestNibeRegister(*nextNibeRegisterToPollSlow);
        nextNibeRegisterToPollSlow++;
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
    uint16_t address = strtoul(topic.c_str() + nibeRootTopic.length(), nullptr, 10);
    if (address == 0) {
        ESP_LOGW(TAG, "Invalid topic %s", topic.c_str());
        return;
    }
    writeNibeRegister(address, payload.c_str());
}

void NibeMqttGw::requestNibeRegister(uint16_t address) {
    if (!xRingbufferSend(readNibeRegistersRingBuffer, &address, sizeof(address), 0)) {
        ESP_LOGW(TAG, "Could not send register %d to readNibeRegistersRingBuffer. Buffer full.", address);
    }
}

void NibeMqttGw::writeNibeRegister(uint16_t address, const char* value) {
    if (value == nullptr) {
        ESP_LOGE(TAG, "writeNibeRegister: value is null for register %d", address);
        return;
    }
    auto valueLen = strlen(value);
    if (valueLen == 0) {
        ESP_LOGE(TAG, "writeNibeRegister: missing value for register %d", address);
        return;
    }
    if (valueLen > sizeof(NibeMqttGwWriteRequest::value) - 1) {
        ESP_LOGE(TAG, "writeNibeRegister: value too long for register %d: %s", address, value);
        return;
    }
    NibeMqttGwWriteRequest writeRequest;
    writeRequest.address = address;
    strncpy(writeRequest.value, value, sizeof(writeRequest.value));
    writeRequest.value[sizeof(writeRequest.value) - 1] = '\0';  // ensure null termination
    if (!xRingbufferSend(writeNibeRegistersRingBuffer, &writeRequest, sizeof(writeRequest), 0)) {
        ESP_LOGE(TAG, "Could not send register %d to writeNibeRegistersRingBuffer. Buffer full.", address);
    }
}

void NibeMqttGw::onMessageReceived(const NibeResponseMessage* const msg, int len) {
    switch (msg->cmd) {
        case NibeCmd::ModbusReadResp: {
            ESP_LOGV(TAG, "onMessageReceived ModbusReadResp: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());
            const NibeRegister* reg = findNibeRegister(msg->readResponse.registerAddress);
            if (reg == nullptr) {
                ESP_LOGW(TAG, "Received NibeResponseMessage for unknown register %d", msg->readResponse.registerAddress);
                return;
            }

            publishMqtt(*reg, msg->readResponse.value);
            publishMetric(*reg, msg->readResponse.value);
            break;
        }

        case NibeCmd::ModbusDataMsg: {
            // ~ one ModMusDataMsg ever 2s, up to 20 registers
            // Limitation: can only handle 16bit registers
            ESP_LOGV(TAG, "onMessageReceived ModbusDataMsg: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());

            int receivedNibeRegisters = 0;
            int publishIdx = modbusDataMsgMqttPublish % 20;
            for (int i = 0; i < 20; i++) {
                NibeDataMessageRegister registerData = msg->dataMessage.registers[i];
                if (registerData.registerAddress == 0xFFFF) {
                    // 0xffff indicates that not all 20 registers in ModbusDataMsg are used
                    break;
                }
                const NibeRegister* _register = findNibeRegister(registerData.registerAddress);
                if (_register == nullptr) {
                    ESP_LOGW(TAG, "Received ModbusDataMsg for unknown register %d", registerData.registerAddress);
                    continue;
                }
                switch (_register->dataType) {
                    case NibeRegisterDataType::UInt8:
                    case NibeRegisterDataType::Int8:
                    case NibeRegisterDataType::UInt16:
                    case NibeRegisterDataType::Int16:
                        break;
                    default:
                        ESP_LOGW(TAG, "Unsupported data type %d in ModbusDataMsg for register %d", (int)_register->dataType, _register->id);
                        continue;
                }

                // publish 2 registers every ModbusDataMsg = every 2s
                // -> ~1 register/s or all 20 registers take ~20s which is around the same speed as polling
                if (i == publishIdx || i == publishIdx + 1) {
                    publishMqtt(*_register, registerData.value);
                }
                publishMetric(*_register, registerData.value);
                receivedNibeRegisters++;
            }
            modbusDataMsgMqttPublish += 2;  // 2 registers published per ModbusDataMsg
            ESP_LOGD(TAG, "onMessageReceived ModbusDataMsg: received %d registers", receivedNibeRegisters);
            break;
        }

        case NibeCmd::ModbusWriteResp: {
            ESP_LOGV(TAG, "onMessageReceived ModbusWriteResp: %s", NibeGw::dataToString((uint8_t*)msg, len).c_str());
            // TODO: would be nice to know which register was written
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

const NibeRegister* NibeMqttGw::findNibeRegister(uint16_t address) {
    auto iter = config->registers.find(address);
    if (iter == config->registers.end()) {
        ESP_LOGW(TAG, "Received data for unknown register %d", address);
        return nullptr;
    }
    return &iter->second;
}

// send registers as mqtt messages, announce new registers for HA auto-discovery
void NibeMqttGw::publishMqtt(const NibeRegister& _register, const uint8_t* const data) {
    // TODO: should check data consistency (len vs data type)
    // decode raw data
    std::string value = _register.decodeData(data);
    // publish data to mqtt
    mqttClient->publish(nibeRootTopic + std::to_string(_register.id), value);

    // announce _register on first appearance
    if (announcedNibeRegisters.find(_register.id) == announcedNibeRegisters.end()) {
        announceNibeRegister(_register);
    }
}

// send registers as metrics, create metric if not exists but only for registers that are configured as metrics
void NibeMqttGw::publishMetric(const NibeRegister& _register, const uint8_t* const data) {
    auto iter2 = nibeRegisterMetrics.find(_register.id);
    if (iter2 == nibeRegisterMetrics.end()) {
        const NibeRegisterMetricConfig& metricCfg = _register.toPromMetricConfig(*config);
        if (metricCfg.isValid()) {
            Metric& metric = metrics.addMetric(metricCfg.name.c_str(), metricCfg.factor, metricCfg.scale, metricCfg.counter);
            iter2 = nibeRegisterMetrics.insert({_register.id, &metric}).first;
        } else {
            // do not publish this register as metric
            iter2 = nibeRegisterMetrics.insert({_register.id, nullptr}).first;
        }
    }
    Metric* metric = iter2->second;
    if (metric != nullptr) {
        int32_t valueInt = _register.decodeDataRaw(data);
        metric->setValue(valueInt);
    }
}

void NibeMqttGw::announceNibeRegister(const NibeRegister& _register) {
    ESP_LOGI(TAG, "Announcing register %u", _register.id);

    auto discoveryDoc = _register.homeassistantDiscoveryMessage(*config, nibeRootTopic, mqttClient->getDeviceDiscoveryInfoRef());
    // !!! if crash (strlen in ROM) -> stack too small (nibegw.h: NIBE_GW_TASK_STACK_SIZE) or incorrect format string!!!
    char discoveryTopic[64];
    const char* component = discoveryDoc["_component_"] | "sensor";
    snprintf(discoveryTopic, sizeof(discoveryTopic), "%s/%s/nibegw/coil-%u/config",
             mqttClient->getConfig().discoveryPrefix.c_str(), component, _register.id);

    discoveryDoc.remove("_component_");
    std::string discoveryMsg;
    serializeJson(discoveryDoc, discoveryMsg);
    mqttClient->publish(discoveryTopic, discoveryMsg, QOS0, true);
    announcedNibeRegisters.insert(_register.id);
}

int NibeMqttGw::onReadTokenReceived(NibeReadRequestMessage* readRequest) {
    size_t item_size;
    uint16_t* readRegisterPtr = (uint16_t*)xRingbufferReceive(readNibeRegistersRingBuffer, &item_size, 0);
    if (readRegisterPtr == nullptr) {
        // no more registers to read
        // calculate time to publish state
        uint32_t startTime = lastPublishStateStartTime.exchange(0);
        if (startTime > 0) {
            uint32_t now = esp_timer_get_time() / 1000;
            metricPublishStateTime.setValue(now - startTime);
        }
        return 0;
    }
    uint16_t address = *readRegisterPtr;
    vRingbufferReturnItem(readNibeRegistersRingBuffer, (void*)readRegisterPtr);

    readRequest->start = NibeStart::Request;
    readRequest->cmd = NibeCmd::ModbusReadReq;
    readRequest->len = 2;
    readRequest->registerAddress = address;
    readRequest->chksum = NibeGw::calcCheckSum((uint8_t*)readRequest, sizeof(NibeReadRequestMessage) - 1);

    ESP_LOGD(TAG, "onReadTokenReceived, read register %d: %s", (int)address,
             NibeGw::dataToString((uint8_t*)readRequest, sizeof(NibeReadRequestMessage)).c_str());
    return sizeof(NibeReadRequestMessage);
}

int NibeMqttGw::onWriteTokenReceived(NibeWriteRequestMessage* writeRequest) {
    size_t item_size;
    NibeMqttGwWriteRequest* writeRegisterPtr = (NibeMqttGwWriteRequest*)xRingbufferReceive(writeNibeRegistersRingBuffer, &item_size, 0);
    if (writeRegisterPtr == nullptr) {
        // no more registers to write
        return 0;
    }
    uint16_t address = writeRegisterPtr->address;
    const NibeRegister* _register = findNibeRegister(address);
    if (_register == nullptr) {
        vRingbufferReturnItem(writeNibeRegistersRingBuffer, (void*)writeRegisterPtr);
        ESP_LOGW(TAG, "Received write request for unknown register %d", address);
        return 0;
    }
    if (_register->mode == NibeRegisterMode::Read) {
        vRingbufferReturnItem(writeNibeRegistersRingBuffer, (void*)writeRegisterPtr);
        ESP_LOGW(TAG, "Received write request for read-only register %d", address);
        return 0;
    }

    writeRequest->start = NibeStart::Request;
    writeRequest->cmd = NibeCmd::ModbusWriteReq;
    writeRequest->len = 6;
    writeRequest->registerAddress = address;
    if (!_register->encodeData(writeRegisterPtr->value, writeRequest->value)) {
        vRingbufferReturnItem(writeNibeRegistersRingBuffer, (void*)writeRegisterPtr);
        return 0;
    }
    writeRequest->chksum = NibeGw::calcCheckSum((uint8_t*)writeRequest, sizeof(NibeWriteRequestMessage) - 1);

    vRingbufferReturnItem(writeNibeRegistersRingBuffer, (void*)writeRegisterPtr);

    ESP_LOGD(TAG, "onWriteTokenReceived for register %d: %s", (int)address,
             NibeGw::dataToString((uint8_t*)writeRequest, sizeof(NibeWriteRequestMessage)).c_str());
    return sizeof(NibeWriteRequestMessage);
}
