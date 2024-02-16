#include <Arduino.h>
#include <ETH.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/task.h>

#include "KMPProDinoESP32.h"
#include "web.h"
#include "config.h"
#include "mqtt.h"
#include "Relay.h"

static const char *TAG = "nibegw";

#define ESP_INIT_NETWORK 0x100
#define ESP_INIT_CONFIG 0x101
#define ESP_INIT_MQTT 0x102
#define ESP_INIT_RELAY 0x103
#define ESP_INIT_TASK 0x104
static esp_err_t init_status = ESP_OK;

void pollingTask(void *pvParameters);

NibeMqttGwConfigManager configManager;
MqttClient mqttClient;
NibeMqttGwWebServer httpServer(80, configManager, mqttClient);

MqttRelay relays[] = {
    MqttRelay("relay1", "Relay 1", Relay1),
    MqttRelay("relay2", "Relay 2", Relay2),
    MqttRelay("relay3", "Relay 3", Relay3),
    MqttRelay("relay4", "Relay 4", Relay4),
};


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

    if (configManager.begin() != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize config manager");
        init_status = ESP_INIT_CONFIG;
    }

    httpServer.begin();

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
    httpServer.setMetricInitStatus(init_status);
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
        httpServer.setMetricPollingTime(runtime);
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
