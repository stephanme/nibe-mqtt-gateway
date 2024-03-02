#ifndef _nibegw_mqtt_h_
#define _nibegw_mqtt_h_

#include <freertos/ringbuf.h>
#include <unordered_set>

#include "mqtt.h"
#include "nibegw.h"
#include "nibegw_config.h"

#define READ_COILS_RING_BUFFER_SIZE 256  // max number of coils to poll

class NibeMqttGw : public NibeGwCallback {
   public:
    NibeMqttGw();

    esp_err_t begin(const NibeMqttConfig& config, MqttClient& mqttClient);

    void publishState();

    // NibeGwCallback
    void onMessageReceived(const uint8_t* const data, int len);
    int onReadTokenReceived(uint8_t* data);
    int onWriteTokenReceived(uint8_t* data);

   private:
    const NibeMqttConfig* config;
    MqttClient* mqttClient;

    std::string nibeRootTopic;
    std::unordered_set<uint16_t> announcedCoils;

    RingbufHandle_t readCoilsRingBuffer;

    void announceCoil(const Coil& coil);
};

#endif