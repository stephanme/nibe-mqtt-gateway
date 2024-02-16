#include "web.h"

#include <ESP.h>
#include <ETH.h>
#include <SPIFFS.h>
#include <Update.h>

#include "KMPProDinoESP32.h"

NibeMqttGwWebServer::NibeMqttGwWebServer(int port, NibeMqttGwConfigManager &configManager, const MqttClient &mqttClient)
    : httpServer(port), configManager(configManager), mqttClient(mqttClient) {}

void NibeMqttGwWebServer::begin() {
    httpServer.begin();

    httpServer.on("/", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetRoot, this));
    httpServer.on("/config", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetConfig, this));
    httpServer.on("/config", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostConfig, this));
    httpServer.on("/metrics", HTTP_GET, std::bind(&NibeMqttGwWebServer::handleGetMetrics, this));
    httpServer.on("/reboot", HTTP_POST, std::bind(&NibeMqttGwWebServer::handlePostReboot, this));

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
<tr><td>Version</td><td>0.1</td></tr>
<tr><td>Hostname</td><td>%s</td></tr>
<tr><td>MQTT Broker</td><td>%s</td></tr>
<tr><td>Init status</td><td>0x%04x</td></tr>
<tr><td>MQTT status</td><td>0x%04x</td></tr>
</table>
<h3>Links</h3>
<ul>
<li><a href="./config">Configuration</a>, POST to set new configuration (triggers reboot)</li>
<li><a href="./update">Firmware Upload</a> (triggers reboot)</li>
<li><a href="./metrics">Metrics</a></li>
</ul>
<form action="./reboot" method="post">
  <button name="reboot" value="reboot">Reboot</button>
</form>
</body>
</html>
)";

void NibeMqttGwWebServer::handleGetRoot() {
    String hostname = ETH.getHostname();
    size_t len = strlen(ROOT_HTML) + hostname.length() + configManager.getConfig().mqtt.brokerUri.length() + 10;
    char rootHtml[len];
    snprintf(rootHtml, len, ROOT_HTML, hostname.c_str(), configManager.getConfig().mqtt.brokerUri.c_str(), init_status,
             mqttClient.status());
    httpServer.send(200, "text/html", rootHtml);
}

void NibeMqttGwWebServer::handleGetConfig() { httpServer.send(200, "application/json", configManager.getConfigAsJson()); }

void NibeMqttGwWebServer::handlePostConfig() {
    String configJson = httpServer.arg("plain");
    if (configManager.saveConfig(configJson) == ESP_OK) {
        httpServer.send(200, "text/plain", "Configuration saved. Rebooting...");
        delay(1000);
        ESP.restart();
    } else {
        // TODO: better err msg
        httpServer.send(400, "text/plain", "Invalid configuration. Check logs.");
    }
}

static const char *REBOOT_HTML = R"(<META http-equiv="refresh" content="5;URL=/">Rebooting...)";

void NibeMqttGwWebServer::handlePostReboot() {
    httpServer.send(200, "text/html", REBOOT_HTML);
    delay(1000);
    ESP.restart();
}

static const char *METRICS_DATA = R"(# nibe_mqtt_gateway metrics
status{category="init"} %d
status{category="mqtt"} %d
esp32_total_free_bytes %lu
esp32_minimum_free_bytes %lu
uptime %lu
polling_time_ms %lu
)";

void NibeMqttGwWebServer::handleGetMetrics() {
    size_t len = strlen(METRICS_DATA) + 256;
    char metrics[len];
    snprintf(metrics, len, METRICS_DATA, init_status, mqttClient.status(), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
             millis() / 1000, pollingTime);
    httpServer.send(200, "text/plain", metrics);
}

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
    httpServer.send_P(200, PSTR("text/html"), OTA_INDEX);
}

void NibeMqttGwWebServer::handlePostUpdate() {
    if (!_authenticated) return httpServer.requestAuthentication();
    if (Update.hasError()) {
        httpServer.send(200, "text/html", String("Update error: ") + _updaterError);
    } else {
        httpServer.client().setNoDelay(true);
        httpServer.send_P(200, "text/html", OTA_SUCCESS);
        delay(100);
        httpServer.client().stop();
        ESP.restart();
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