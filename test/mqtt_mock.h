#include "mqtt.h"
#include <vector>

struct MqttPublishData {
    std::string topic;
    std::string payload;
    MqttQOS qos;
    bool retain;
};

extern std::vector<MqttPublishData> mqttmock_publishData;