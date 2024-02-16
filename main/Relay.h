#ifndef _Relay_h_
#define _Relay_h_

#include <Arduino.h>

#include "KMPProDinoESP32.h"
#include "mqtt.h"

class MqttRelay : MqttSubscriptionCallback {
   public:
    MqttRelay(const String& mqttTopic, const String& name, Relay relay);

    int begin(MqttClient* mqttClient);

    void setRelayState(bool state);
    bool getRelayState(void);
    void publishState();

    const String& getName(void) { return name; }

   private:
    String mqttTopic;
    String name;
    enum Relay relay;
    MqttClient* mqttClient;

    String stateTopic;

    void onMqttMessage(const String& topic, const String& payload);
    void publishState(bool state);
};

#endif
