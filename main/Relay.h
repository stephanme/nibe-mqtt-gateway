#ifndef _Relay_h_
#define _Relay_h_

#include "config.h"
// include config.h before ArduinoJson.h to enable comments in JSON
#include <ArduinoJson.h>

#include <string>

#include "KMPProDinoESP32.h"
#include "mqtt.h"
#include "metrics.h"

struct MqttRelayConfig {
    std::string name;
    std::string homeassistantDiscoveryOverride;
};

class MqttRelay : MqttSubscriptionCallback {
   public:
    MqttRelay(Relay relay, const std::string& name, Metrics& metrics);

    int begin(const MqttRelayConfig& config, MqttClient* mqttClient);

    void setRelayState(bool state);
    bool getRelayState(void);
    void publishState();

    const std::string& getName(void) { return name; }
    const std::string& getStateTopic(void) { return stateTopic; }

    // public for for testing only
    JsonDocument homeassistantDiscoveryMessage(const MqttRelayConfig& config, const std::string& nibeRootTopic,
                                               const std::string& deviceDiscoveryInfo) const;
    JsonDocument defaultHomeassistantDiscoveryMessage(const std::string& nibeRootTopic,
                                                      const std::string& deviceDiscoveryInfo) const;

   private:
    std::string name;
    enum Relay relay;
    MqttClient* mqttClient;
    std::string stateTopic;

    Metrics& metrics;
    Metric* metricRelayState = nullptr;

    void onMqttMessage(const std::string& topic, const std::string& payload);
    void publishState(bool state);
};

#endif
