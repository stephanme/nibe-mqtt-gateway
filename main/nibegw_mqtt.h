#ifndef _nibegw_mqtt_h_
#define _nibegw_mqtt_h_

#include <freertos/ringbuf.h>

#include <unordered_set>

#include "mqtt.h"
#include "nibegw.h"
#include "nibegw_config.h"

#define READ_REGISTER_RING_BUFFER_SIZE 256  // max number of pending registers to poll
#define WRITE_REGISTER_RING_BUFFER_SIZE 16  // max number of pending registers to write

class NibeMqttGw : public NibeGwCallback, MqttSubscriptionCallback {
   public:
    NibeMqttGw(Metrics& metrics);

    esp_err_t begin(const NibeMqttConfig& config, MqttClient& mqttClient);

    // request and publish configured registers to mqtt
    void publishState();
    // request and publish a single register
    void requestNibeRegister(uint16_t address);
    // write a single register
    void writeNibeRegister(uint16_t address, const char* str);

    // NibeGwCallback
    void onMessageReceived(const NibeResponseMessage* const msg, int len);
    int onReadTokenReceived(NibeReadRequestMessage* data);
    int onWriteTokenReceived(NibeWriteRequestMessage* data);
    // MqttSubscriptionCallback
    void onMqttMessage(const std::string& topic, const std::string& payload);

   private:
    Metrics& metrics;
    const NibeMqttConfig* config;
    MqttClient* mqttClient;

    std::string nibeRootTopic;
    std::unordered_set<uint16_t> announcedNibeRegisters;
    std::unordered_map<uint16_t, Metric*> nibeRegisterMetrics;
    int modbusDataMsgMqttPublish;

    RingbufHandle_t readNibeRegistersRingBuffer;
    RingbufHandle_t writeNibeRegistersRingBuffer;

    Metric& metricPublishStateTime;
    std::atomic<uint32_t> lastPublishStateStartTime;
    std::vector<uint16_t>::const_iterator nextNibeRegisterToPollSlow;
    int numNibeRegistersToPoll;

    const NibeRegister* findNibeRegister(uint16_t address);
    void publishMetric(const NibeRegister& _register, const uint8_t* const data);
    void publishMqtt(const NibeRegister& _register, const uint8_t* const data);
    void announceNibeRegister(const NibeRegister& _register);
};

struct NibeMqttGwWriteRequest {
    uint16_t address;
    char value[16];  // zero terminated string
};

#endif