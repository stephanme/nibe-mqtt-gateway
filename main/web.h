#ifndef _WEB_H_
#define _WEB_H_

#include <Arduino.h>
#include <WebServer.h>

#include "config.h"
#include "mqtt.h"

class NibeMqttGwWebServer {
   public:
    NibeMqttGwWebServer(int port, NibeMqttGwConfigManager& configManager, const MqttClient& mqttClient);

    void begin();
    void handleClient();

    void setMetricInitStatus(unsigned long init_status) { this->init_status = init_status; }
    void setMetricPollingTime(unsigned long pollingTime) { this->pollingTime = pollingTime; }

   private:
    WebServer httpServer;

    NibeMqttGwConfigManager& configManager;
    const MqttClient& mqttClient;

    // metrics
    esp_err_t init_status;
    unsigned long pollingTime;

    // TODO: auth configuration
    String _username = emptyString;
    String _password = emptyString;
    bool _authenticated = false;
    String _updaterError;

    void handleGetRoot();
    void handleGetConfig();
    void handlePostConfig();
    void handlePostReboot();
    void handleGetMetrics();
    void handleNotFound();

    void handleGetUpdate();
    void handlePostUpdate();
    void handlePostUpload();
    void setUpdaterError();
};

#endif