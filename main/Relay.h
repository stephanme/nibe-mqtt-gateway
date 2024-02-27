#ifndef _Relay_h_
#define _Relay_h_

#include <Arduino.h>

#include <string>

#include "KMPProDinoESP32.h"
#include "mqtt.h"

class MqttRelay : MqttSubscriptionCallback {
   public:
    MqttRelay(const std::string& mqttTopic, const std::string& name, Relay relay);

    int begin(MqttClient* mqttClient);

    void setRelayState(bool state);
    bool getRelayState(void);
    void publishState();

    const std::string& getName(void) { return name; }

   private:
    std::string mqttTopic;
    std::string name;
    enum Relay relay;
    MqttClient* mqttClient;

    std::string stateTopic;

    void onMqttMessage(const std::string& topic, const std::string& payload);
    void publishState(bool state);
};

#endif
