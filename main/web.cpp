#include "web.h"

#include <ESP.h>
#include <ETH.h>
#include <SPIFFS.h>
#include <Update.h>
#include <esp_app_desc.h>

#include "config.h"
#include "KMPProDinoESP32.h"

#define ROOT_REDIRECT_HTML R"(<META http-equiv="refresh" content="5;URL=/">)"
#define NIBE_MODBUS_UPLOAD_FILE "/nibe_modbus_upload.csv"

static const char *TAG = "web";

NibeMqttGwWebServer::NibeMqttGwWebServer(int port, Metrics &metrics, NibeMqttGwConfigManager &configManager,
                                         NibeMqttGw &nibeMqttGw, EnergyMeter &energyMeter)
    : httpServer(port), metrics(metrics), configManager(configManager), nibeMqttGw(nibeMqttGw), energyMeter(energyMeter) {}

void NibeMqttGwWebServer::begin() {
    // init order ensures that metrics exist
    metricInitStatus = metrics.findMetric(METRIC_NAME_INIT_STATUS);
    assert(metricInitStatus != nullptr);
    metricMqttStatus = metrics.findMetric(METRIC_NAME_MQTT_STATUS);
    assert(metricMqttStatus != nullptr);

    httpServer.begin();

    httpServer.on("/", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetRoot, this));
    httpServer.on("/config", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetConfig, this));
    httpServer.on("/config", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostConfig, this));
    httpServer.on("/config/nibe", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetNibeConfig, this));
    httpServer.on("/config/nibe", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostNibeConfig, this),
                  std::bind(&NibeMqttGwWebServer::handlePostNibeConfigUpload, this));
    httpServer.on("/config/energymeter", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostEnergyMeter, this));
    httpServer.on("/metrics", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetMetrics, this));
    httpServer.on("/reboot", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostReboot, this));
    httpServer.on("/coil", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostCoil, this));

    httpServer.on("/update", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetUpdate, this));
    httpServer.on("/update", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostUpdate, this),
                  std::bind(&NibeMqttGwWebServer::handlePostUpload, this));

    httpServer.onNotFound(std::bind(&NibeMqttGwWebServer::handleNotFound, this));
}

void NibeMqttGwWebServer::handleClient() { httpServer.handleClient(); }

static const char *ROOT_HTML = R"(<!DOCTYPE html>
<html lang='en'>
<head>
<meta charset='utf-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'/>
</head>
<body>
<h1>Nibe MQTT Gateway</h1>
<h3>Info</h3>
<table>
<tr><td>Version</td><td>%s</td></tr>
<tr><td>Hostname</td><td>%s</td></tr>
<tr><td>MQTT Broker</td><td>%s</td></tr>
<tr><td>Init status</td><td>0x%04x</td></tr>
<tr><td>MQTT status</td><td>0x%04x</td></tr>
</table>
<h3>Links</h3>
<ul>
<li><a href="./config">Configuration</a>, POST to set new configuration (triggers reboot)</li>
<li><a href="./config/nibe">Nibe Modbus Configuration</a>, POST to set new configuration (triggers reboot)</li>
<li><a href="./update">Firmware Upload</a> (triggers reboot)</li>
<li><a href="./metrics">Metrics</a></li>
</ul>
<h3>Actions</h3>
<form action="./reboot" method="post">
  <button name="reboot" value="reboot">Reboot</button>
</form>
<br/>
<form action="./coil" method="post">
  <label for="coil">Nibe coil: </label>
  <input type="text" id="coil" name="coil" value="">
  <button>Request</button>
</form>
<br/>
<form action="./config/energymeter" method="post">
  <label for="energy">Energy meter [Wh]: </label>
  <input type="text" id="energy" name="plain" value="">
  <button>Set</button>
</form>
</body>
</html>
)";

void NibeMqttGwWebServer::handleGetRoot() {
    String hostname = ETH.getHostname();
    const esp_app_desc_t *app_desc = esp_app_get_description();
    size_t len = strlen(ROOT_HTML) + strlen(app_desc->version) + hostname.length() +
                 configManager.getConfig().mqtt.brokerUri.length() + 10;
    char rootHtml[len];
    uint16_t initStatus = (uint16_t)metricInitStatus->getValue();
    uint16_t mqttStatus = (uint16_t)metricMqttStatus->getValue();
    snprintf(rootHtml, len, ROOT_HTML, app_desc->version, hostname.c_str(), configManager.getConfig().mqtt.brokerUri.c_str(),
             initStatus, mqttStatus);
    httpServer.send(200, "text/html", rootHtml);
}

void NibeMqttGwWebServer::handleGetConfig() { httpServer.send(200, "application/json", configManager.getConfigAsJson().c_str()); }

void NibeMqttGwWebServer::handlePostConfig() {
    if (configManager.saveConfig(httpServer.arg("plain").c_str()) == ESP_OK) {
        send200AndReboot(ROOT_REDIRECT_HTML "Configuration saved. Rebooting...");
    } else {
        // TODO: better err msg
        httpServer.send(400, "text/plain", "Invalid configuration. Check logs.");
    }
}

void NibeMqttGwWebServer::handleGetNibeConfig() {
    httpServer.send(200, "text/plain", configManager.getNibeModbusConfig().c_str());
}

void NibeMqttGwWebServer::handlePostNibeConfigUpload() {
    HTTPUpload &upload = httpServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        _updaterError.clear();
        ESP_LOGI(TAG, "Upload nibe config: %s", upload.filename.c_str());
        nibeConfigUploadFile = LittleFS.open(NIBE_MODBUS_UPLOAD_FILE, FILE_WRITE);
        if (!nibeConfigUploadFile) {
            setNibeConfigUpdateError("Failed to open file: " NIBE_MODBUS_UPLOAD_FILE);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
        if (nibeConfigUploadFile.write(upload.buf, upload.currentSize) != upload.currentSize) {
            setNibeConfigUpdateError("Failed to write file: " NIBE_MODBUS_UPLOAD_FILE);
        }
    } else if (upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
        ESP_LOGI(TAG, "Upload nibe config finished: %u bytes", upload.totalSize);
        nibeConfigUploadFile.close();

        if (configManager.saveNibeModbusConfig(NIBE_MODBUS_UPLOAD_FILE) != ESP_OK) {
            setNibeConfigUpdateError("Invalid nibe modbus configuration. Check logs.");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        ESP_LOGW(TAG, "Nibe config upload was aborted");
        nibeConfigUploadFile.close();
    }
    delay(0);
}

void NibeMqttGwWebServer::setNibeConfigUpdateError(const char *err) {
    _updaterError = err;
    ESP_LOGE(TAG, "%s", _updaterError.c_str());
}

void NibeMqttGwWebServer::handlePostNibeConfig() {
    if (_updaterError.isEmpty()) {
        send200AndReboot(ROOT_REDIRECT_HTML "Nibe Modbus configuration saved. Rebooting...");
    } else {
        // TODO: better err msg
        httpServer.send(400, "text/plain", _updaterError);
    }
}

void NibeMqttGwWebServer::handlePostEnergyMeter() {
    long wh = httpServer.arg("plain").toInt();
    if (wh > 0) {
        ESP_LOGI(TAG, "Set energy meter to %ld Wh", wh);
        energyMeter.setEnergyInWh(wh);
        httpServer.send(200, "text/html", ROOT_REDIRECT_HTML "Energy value set");
    } else {
        httpServer.send(400, "text/plain", "Invalid energy value");
    }
}

void NibeMqttGwWebServer::handlePostReboot() { send200AndReboot(ROOT_REDIRECT_HTML "Rebooting..."); }

void NibeMqttGwWebServer::handlePostCoil() {
    String coil = httpServer.arg("coil");
    uint16_t coilAddress = coil.toInt();
    if (coilAddress > 0) {
        ESP_LOGI(TAG, "Requesting coil %d", coilAddress);
        nibeMqttGw.requestCoil(coilAddress);
        httpServer.send(200, "text/html", ROOT_REDIRECT_HTML "Sent request to Nibe for coil " + coil);
    } else {
        httpServer.send(400, "text/plain", "Bad coil parameter: " + coil);
    }
}

void NibeMqttGwWebServer::handleGetMetrics() { httpServer.send(200, "text/plain", metrics.getAllMetricsAsString().c_str()); }

static const char *NOT_FOUND_MSG = R"(File Not Found

%s: %s
)";

// must match HTTPMethod enum
static const char *HTTP_METHOD_NAMES[] = {"DELETE", "GET", "HEAD", "POST", "PUT"};

void NibeMqttGwWebServer::handleNotFound() {
    size_t len = strlen(NOT_FOUND_MSG) + httpServer.uri().length() + 8;
    char message[len];
    HTTPMethod method = httpServer.method();
    const char *methodName;
    if (method <= HTTP_PUT) {
        methodName = HTTP_METHOD_NAMES[method];
    } else {
        methodName = "???";
    }
    snprintf(message, len, NOT_FOUND_MSG, methodName, httpServer.uri().c_str());
    httpServer.send(404, "text/plain", message);
}

// OTA
static const char *OTA_TAG = "ota";

static const char OTA_INDEX[] =
    R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body>
     <h1><h1>Nibe MQTT Gateway - OTA</h1>
     <form method='POST' action='' enctype='multipart/form-data'>
         Firmware:<br>
         <input type='file' accept='.bin,.bin.gz' name='firmware'>
         <input type='submit' value='Update Firmware'>
     </form>
     <form method='POST' action='' enctype='multipart/form-data'>
         FileSystem:<br>
         <input type='file' accept='.bin,.bin.gz,.image' name='filesystem'>
         <input type='submit' value='Update FileSystem'>
     </form>
     </body>
     </html>)";
static const char OTA_SUCCESS[] = R"(<META http-equiv="refresh" content="15;URL=/">Update Success! Rebooting...)";

void NibeMqttGwWebServer::setUpdaterError() {
    _updaterError = Update.errorString();
    ESP_LOGE(OTA_TAG, "%s", _updaterError.c_str());
}

void NibeMqttGwWebServer::handleGetUpdate() {
    if (_username != emptyString && _password != emptyString && !httpServer.authenticate(_username.c_str(), _password.c_str()))
        return httpServer.requestAuthentication();
    httpServer.send(200, "text/html", OTA_INDEX);
}

void NibeMqttGwWebServer::handlePostUpdate() {
    if (!_authenticated) return httpServer.requestAuthentication();
    if (Update.hasError()) {
        httpServer.send(200, "text/html", String("Update error: ") + _updaterError);
    } else {
        send200AndReboot(OTA_SUCCESS);
    }
}

// handler for the file upload, get's the sketch bytes, and writes
// them through the Update object
void NibeMqttGwWebServer::handlePostUpload() {
    HTTPUpload &upload = httpServer.upload();

    if (upload.status == UPLOAD_FILE_START) {
        _updaterError.clear();

        _authenticated = (_username == emptyString || _password == emptyString ||
                          httpServer.authenticate(_username.c_str(), _password.c_str()));
        if (!_authenticated) {
            ESP_LOGE(OTA_TAG, "Unauthenticated Update");
            return;
        }

        ESP_LOGI(OTA_TAG, "Update: %s", upload.filename.c_str());
        if (upload.name == "filesystem") {
            if (!Update.begin(SPIFFS.totalBytes(), U_SPIFFS)) {  // start with max available size
                ESP_LOGE(OTA_TAG, "%s", Update.errorString());
            }
        } else {
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace, U_FLASH)) {  // start with max available size
                setUpdaterError();
            }
        }
    } else if (_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
        // could add blinking LED here
        KMPProDinoESP32.processStatusLed(red, 1000);

        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            setUpdaterError();
        }
    } else if (_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
        ESP_LOGI(OTA_TAG, "Upload finished: %u bytes", upload.totalSize);
        KMPProDinoESP32.setStatusLed(red);
        if (Update.end(true)) {  // true to set the size to the current progress
            ESP_LOGI(OTA_TAG, "Update Success - Rebooting...");
        } else {
            setUpdaterError();
        }
    } else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        ESP_LOGW(OTA_TAG, "Update was aborted");
    }
    delay(0);
}

void NibeMqttGwWebServer::send200AndReboot(const char *msg) {
    httpServer.client().setNoDelay(true);
    httpServer.send(200, "text/html", msg);
    delay(1000);
    httpServer.client().stop();
    delay(1000);
    ESP.restart();
}