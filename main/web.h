#ifndef _WEB_H_
#define _WEB_H_

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>

#include "config.h"
#include "energy_meter.h"
#include "mqtt.h"

class NibeMqttGwWebServer {
   public:
    NibeMqttGwWebServer(int port, NibeMqttGwConfigManager& configManager, const MqttClient& mqttClient, EnergyMeter& energyMeter);

    void begin();
    void handleClient();

    void setMetricInitStatus(unsigned long init_status) { this->init_status = init_status; }
    void setMetricPollingTime(unsigned long pollingTime) { this->pollingTime = pollingTime; }

   private:
    WebServer httpServer;

    NibeMqttGwConfigManager& configManager;
    const MqttClient& mqttClient;
    EnergyMeter& energyMeter;

    // metrics
    esp_err_t init_status;
    unsigned long pollingTime;

    // TODO: auth configuration
    String _username = emptyString;
    String _password = emptyString;
    bool _authenticated = false;
    String _updaterError;

    File nibeConfigUploadFile;

    void handleGetRoot();
    void handleGetConfig();
    void handlePostConfig();
    void handleGetNibeConfig();
    void handlePostNibeConfig();
    void handlePostNibeConfigUpload();
    void setNibeConfigUpdateError(const char* err);
    void handlePostEnergyMeter();
    void handlePostReboot();
    void handleGetMetrics();
    void handleNotFound();

    void handleGetUpdate();
    void handlePostUpdate();
    void handlePostUpload();
    void setUpdaterError();

    void send200AndReboot(const char* msg);
};

#endif