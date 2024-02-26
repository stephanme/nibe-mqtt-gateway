#ifndef _nibegw_mqtt_h_
#define _nibegw_mqtt_h_

#include "mqtt.h"
#include "nibegw.h"

#include <freertos/ringbuf.h>

#define READ_COILS_RING_BUFFER_SIZE 256  // max number of coils to poll

// configuration
enum CoilDataType {
    COIL_DATA_TYPE_UINT8,
    COIL_DATA_TYPE_INT8,
    COIL_DATA_TYPE_UINT16,
    COIL_DATA_TYPE_INT16,
    COIL_DATA_TYPE_UINT32,
    COIL_DATA_TYPE_INT32,
    COIL_DATA_TYPE_DATE,
};

struct Coil {
    uint16_t address;
    String name;
    String description;
    CoilDataType dataType;
    int scaleFactor;
};

struct NibeMqttConfig {
    std::unordered_map<uint16_t, Coil> coils;
    std::vector<uint16_t> coilsToPoll;
};

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

    RingbufHandle_t readCoilsRingBuffer;

    static String dataToString(const uint8_t* const data, int len);
    static String decodeCoilData(const Coil& coil, const NibeReadResponseData& data);
    static String formatNumber(const Coil& coil, auto value);
};

#endif