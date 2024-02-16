#include <Arduino.h>
#include <ESP.h>
#include <ETH.h>
#include <WebServer.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/task.h>

#include "HTTPUpdateServer.h"
#include "KMPProDinoESP32.h"
#include "Relay.h"
#include "config.h"
#include "mqtt.h"

static const char *TAG = "nibegw";

#define ESP_INIT_NETWORK 0x100
#define ESP_INIT_CONFIG 0x101
#define ESP_INIT_MQTT 0x102
#define ESP_INIT_RELAY 0x103
#define ESP_INIT_TASK 0x104
static esp_err_t init_status = ESP_OK;

void pollingTask(void *pvParameters);
unsigned long lastPollingTime = 0;

WebServer httpServer(80);
HTTPUpdateServer httpUpdater;

NibeMqttGwConfigManager configManager;
MqttClient mqttClient;

MqttRelay relays[] = {
    MqttRelay("relay1", "Relay 1", Relay1),
    MqttRelay("relay2", "Relay 2", Relay2),
    MqttRelay("relay3", "Relay 3", Relay3),
    MqttRelay("relay4", "Relay 4", Relay4),
};

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

void handleRoot() {
    String hostname = ETH.getHostname();
    size_t len = strlen(ROOT_HTML) + hostname.length() + configManager.getConfig().mqtt.brokerUri.length() + 10;
    char rootHtml[len];
    snprintf(rootHtml, len, ROOT_HTML, hostname.c_str(), configManager.getConfig().mqtt.brokerUri.c_str(), init_status,
             mqttClient.status());
    httpServer.send(200, "text/html", rootHtml);
}

void handleGetConfig() { httpServer.send(200, "application/json", configManager.getConfigAsJson()); }

void handlePostConfig() {
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

void handleReboot() {
    httpServer.send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

static const char *METRICS_DATA = R"(# nibe_mqtt_gateway metrics
status{category=init} %d
status{category=mqtt} %d
esp32_total_free_bytes %lu
esp32_minimum_free_bytes %lu
uptime %lu
polling_time_ms %lu
)";

void handleMetrics() {
    size_t len = strlen(METRICS_DATA) + 256;
    char metrics[len];
    snprintf(metrics, len, METRICS_DATA, init_status, mqttClient.status(), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
             millis() / 1000, lastPollingTime);
    httpServer.send(200, "text/plain", metrics);
}

static const char *NOT_FOUND_MSG = R"(File Not Found

%s: %s
)";

// must match HTTPMethod enum
static const char *HTTP_METHOD_NAMES[] = {"DELETE", "GET", "HEAD", "POST", "PUT"};

void handleNotFound() {
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

void setup() {
    delay(1000);

    Serial.begin(115200);
    // wait for serial port to connect. Needed for native USB
    while (!Serial) {
        delay(100);
    }
    ESP_LOGI(TAG, "Nibe MQTT Gateway is starting...");

    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet);
    KMPProDinoESP32.setStatusLed(blue);
    // disable GPIO logging
    esp_log_level_set("gpio", ESP_LOG_WARN);

    httpUpdater.setup(&httpServer);
    httpServer.on("/", HTTP_GET, handleRoot);
    httpServer.on("/config", HTTP_GET, handleGetConfig);
    httpServer.on("/config", HTTP_POST, handlePostConfig);
    httpServer.on("/metrics", HTTP_GET, handleMetrics);
    httpServer.on("/reboot", HTTP_POST, handleReboot);
    httpServer.onNotFound(handleNotFound);
    httpServer.begin();

    if (configManager.begin() != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize config manager");
        init_status = ESP_INIT_CONFIG;
    }

    // wait for network
    while (!ETH.hasIP()) {
        KMPProDinoESP32.processStatusLed(blue, 1000);
        delay(500);
    }

    // build clientId from hostname and mac address
    esp_err_t err = mqttClient.begin(configManager.getConfig().mqtt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize MQTT client");
        init_status = ESP_INIT_MQTT;
    }

    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        err = relays[i].begin(&mqttClient);
        if (err != 0) {
            ESP_LOGE(TAG, "Could not initialize relay %d", i);
            init_status = ESP_INIT_RELAY;
        }
    }

    // start polling task
    // Prios: idle=0, arduino=1, mqtt=5 (default), polling=10
    err = xTaskCreatePinnedToCore(&pollingTask, "pollingTask", 4 * 1024, NULL, 10, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start polling task");
        init_status = ESP_INIT_TASK;
    }

    KMPProDinoESP32.offStatusLed();
    ESP_LOGI(TAG, "Nibe MQTT Gateway is started. Status: %x", init_status);
}

void pollingTask(void *pvParameters) {
    while (1) {
        unsigned long start_time = millis();
        mqttClient.publishAvailability();

        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            relays[i].publishState();
        }

        // measure runtime and calculate delay
        unsigned long runtime = millis() - start_time;
        lastPollingTime = runtime;
        if (runtime < 30000) {
            delay(30000 - runtime);
        } else {
            ESP_LOGW(TAG, "Polling task runtime: %lu ms", runtime);
            delay(1000);
        }
    }
}

void loop() {
    if (init_status == ESP_OK && mqttClient.status() == ESP_OK) {
        KMPProDinoESP32.processStatusLed(green, 1000);
    } else {
        KMPProDinoESP32.processStatusLed(orange, 1000);
    }
    httpServer.handleClient();
    delay(2);
}
