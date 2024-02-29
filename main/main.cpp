#include <Arduino.h>
#include <ETH.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/task.h>
#include <esp_app_desc.h>


#include "KMPProDinoESP32.h"
#include "web.h"
#include "config.h"
#include "mqtt.h"
#include "Relay.h"
#include "nibegw_mqtt.h"

#define RS485_RX_PIN          4
#define RS485_TX_PIN          16
#define RS485_DIRECTION_PIN   2

static const char *TAG = "main";

#define ESP_INIT_NETWORK 0x100
#define ESP_INIT_CONFIG 0x101
#define ESP_INIT_LOGGING 0x102
#define ESP_INIT_MQTT 0x103
#define ESP_INIT_RELAY 0x104
#define ESP_INIT_TASK 0x105
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

NibeMqttGw nibeMqttGw;
// NibeGw nibegw(&RS485Serial, RS485_DIRECTION_PIN, RS485_RX_PIN, RS485_TX_PIN);
SimulatedNibeGw nibegw;

void setup() {
    esp_err_t err;
    delay(1000);

    Serial.begin(115200);
    // wait for serial port to connect. Needed for native USB
    while (!Serial) {
        delay(100);
    }

    KMPProDinoESP32.begin(ProDino_ESP32_Ethernet);
    KMPProDinoESP32.setStatusLed(blue);
    // disable GPIO logging
    esp_log_level_set("gpio", ESP_LOG_WARN);

    // early init of logging
    if (configManager.begin() != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize config manager");
        init_status = ESP_INIT_CONFIG;
    }
    const NibeMqttGwConfig& config = configManager.getConfig();
    if (config.logging.mqttLoggingEnabled) {
        err = MqttLogging::begin(config.logging, mqttClient);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not initialize MQTT logging");
            init_status = ESP_INIT_LOGGING;
        }
    }

    ESP_LOGI(TAG, "Nibe MQTT Gateway is starting...");
    const esp_app_desc_t *app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "version=%s, idf_ver=%s", app_desc->version, app_desc->idf_ver);

    // start nibegw
    nibeMqttGw.begin(config.nibe, mqttClient);
    nibegw.begin(nibeMqttGw);

    httpServer.begin();

    // wait for network
    while (!ETH.hasIP()) {
        KMPProDinoESP32.processStatusLed(blue, 1000);
        delay(500);
    }

    // build clientId from hostname and mac address
    err = mqttClient.begin(config.mqtt);
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
    // Prios: idle=0, main_app/arduino setup/loop=1, mqtt_logging=4, mqtt=5 (default), polling=10, nibegw=15
    err = xTaskCreatePinnedToCore(&pollingTask, "pollingTask", 4 * 1024, NULL, 10, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start polling task");
        init_status = ESP_INIT_TASK;
    }

    KMPProDinoESP32.offStatusLed();
    ESP_LOGI(TAG, "Nibe MQTT Gateway is started. Status: %x, took %lu ms", init_status, millis());
    httpServer.setMetricInitStatus(init_status);
}

void pollingTask(void *pvParameters) {
    while (1) {
        unsigned long start_time = millis();
        mqttClient.publishAvailability();

        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            relays[i].publishState();
        }

        nibeMqttGw.publishState();

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
