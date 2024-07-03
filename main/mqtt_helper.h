#ifndef _mqtt_helper_h_
#define _mqtt_helper_h_

#include <string>

#include "config.h"
//
#include <ArduinoJson.h>

class MqttHelper {
   public:
    static bool matchTopic(const char* topic, const char* filter);
    static void mergeMqttDiscoveryInfoOverride(JsonDocument& discoveryDoc, const std::string& override);
};

#endif