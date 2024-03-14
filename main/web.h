#ifndef _WEB_H_
#define _WEB_H_

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>

#include "configmgr.h"
#include "energy_meter.h"
#include "metrics.h"
#include "nibegw_mqtt.h"

class NibeMqttGwWebServer {
   public:
    NibeMqttGwWebServer(int port, Metrics& metrics, NibeMqttGwConfigManager& configManager, NibeMqttGw& nibeMqttGw,
                        EnergyMeter& energyMeter);

    void begin();
    void handleClient();

   private:
    WebServer httpServer;

    Metrics& metrics;
    NibeMqttGwConfigManager& configManager;
    NibeMqttGw& nibeMqttGw;
    EnergyMeter& energyMeter;

    // metrics needed for UI
    Metric* metricInitStatus;
    Metric* metricMqttStatus;

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
    void handlePostLogLevel();
    void handlePostReboot();
    void handlePostCoil();

    void handleGetMetrics();
    void handleNotFound();

    void handleGetUpdate();
    void handlePostUpdate();
    void handlePostUpload();
    void setUpdaterError();

    void send200AndReboot(const char* msg);
};

#endif