#include <Arduino.h>
#include <ETH.h>
#include <esp_app_desc.h>
#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/task.h>
#include <nvs.h>

#include "KMPProDinoESP32.h"
#include "Relay.h"
#include "config.h"
#include "configmgr.h"
#include "energy_meter.h"
#include "metrics.h"
#include "mqtt.h"
#include "nibegw_mqtt.h"
#include "nibegw_rs485.h"
#include "web.h"

#define RS485_RX_PIN 4
#define RS485_TX_PIN 16
#define RS485_DIRECTION_PIN 2

#define NIBEGW_NVS_KEY_BOOT_COUNT "bootCount"

static const char* TAG = "main";

enum class InitStatus {
    Uninitialized = -1,
    OK = 0,
    SafeBoot = 1,

    ErrNvs = 0x100,
    ErrNetwork,
    ErrConfigMgr,
    ErrLogging,
    ErrMqtt,
    ErrRelay,
    ErrPollingTask,
    ErrEnergyMeter,
    ErrEnergyMeterMqtt,
    ErrNibeMqttGw,
    ErrNibeGw,
};

Metrics metrics;
NibeMqttGwConfigManager configManager;
MqttClient mqttClient(metrics);

MqttRelay relays[] = {
    MqttRelay("relay1", "Relay 1", Relay1),
    MqttRelay("relay2", "Relay 2", Relay2),
    MqttRelay("relay3", "Relay 3", Relay3),
    MqttRelay("relay4", "Relay 4", Relay4),
};

EnergyMeter energyMeter(metrics);

NibeMqttGw nibeMqttGw(metrics);
NibeGw nibegw(&RS485Serial, RS485_DIRECTION_PIN, RS485_RX_PIN, RS485_TX_PIN);
// SimulatedNibeGw nibegw;

NibeMqttGwWebServer httpServer(80, metrics, configManager, nibeMqttGw, energyMeter);

Metric& metricInitStatus = metrics.addMetric(METRIC_NAME_INIT_STATUS, 1);
Metric& metricTotalFreeBytes = metrics.addMetric(R"(nibegw_total_free_bytes)", 1);
Metric& metricMinimumFreeBytes = metrics.addMetric(R"(nibegw_minimum_free_bytes)", 1);
Metric& metricUptime = metrics.addMetric(R"(nibegw_uptime_seconds_total)", 1);
Metric& metricPollingTime = metrics.addMetric(R"(nibegw_task_runtime_seconds{task="pollingTask"})", 1000);

nvs_handle_t nvsHandle;
uint32_t bootCounter = 0;

void setupSafeBoot();
void setupNormalBoot();
void pollingTask(void* pvParameters);

void setup() {
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

    metrics.begin();
    metricInitStatus.setValue((int32_t)InitStatus::Uninitialized);

    // detect crash loop and enter safe boot mode
    bool safeBoot = true;
    esp_err_t err = nvs_open(NIBEGW_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err == ESP_OK) {
        err = nvs_get_u32(nvsHandle, NIBEGW_NVS_KEY_BOOT_COUNT, &bootCounter);
        if (err == ESP_OK && bootCounter < 3) {
            safeBoot = false;
            nvs_set_u32(nvsHandle, NIBEGW_NVS_KEY_BOOT_COUNT, ++bootCounter);
            nvs_commit(nvsHandle);
        }
    } else {
        ESP_LOGE(TAG, "nvs_open failed: %d", err);
        metricInitStatus.setValue((int32_t)InitStatus::ErrNvs);
    }

    if (safeBoot) {
        setupSafeBoot();
    } else {
        setupNormalBoot();
    }

    ESP_LOGI(TAG, "Nibe MQTT Gateway is running. Status: %lx, took %lu ms", metricInitStatus.getValue(), millis());
    KMPProDinoESP32.offStatusLed();
}

void setupSafeBoot() {
    ESP_LOGW(TAG, "Starting in safe boot mode");
    metricInitStatus.setValue((int32_t)InitStatus::SafeBoot);

    httpServer.begin();

    // wait for network
    while (!ETH.hasIP()) {
        KMPProDinoESP32.processStatusLed(blue, 1000);
        delay(500);
    }
}

void setupNormalBoot() {
    metricInitStatus.setValue((int32_t)InitStatus::OK);
    esp_err_t err;
    // early init of logging
    if (configManager.begin() != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize config manager");
        metricInitStatus.setValue((int32_t)InitStatus::ErrConfigMgr);
    }
    const NibeMqttGwConfig& config = configManager.getConfig();
    if (config.logging.mqttLoggingEnabled) {
        err = MqttLogging::begin(config.logging, mqttClient);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Could not initialize MQTT logging");
            metricInitStatus.setValue((int32_t)InitStatus::ErrLogging);
        }
    }

    ESP_LOGI(TAG, "Nibe MQTT Gateway is starting...");
    const esp_app_desc_t* app_desc = esp_app_get_description();
    ESP_LOGI(TAG, "version=%s, idf_ver=%s", app_desc->version, app_desc->idf_ver);

    // energy meter, init early to not miss pulses on reboot
    err = energyMeter.begin();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize energy meter");
        metricInitStatus.setValue((int32_t)InitStatus::ErrEnergyMeter);
    }

    httpServer.begin();

    // wait for network
    while (!ETH.hasIP()) {
        KMPProDinoESP32.processStatusLed(blue, 1000);
        delay(500);
    }

    // mqtt client
    err = mqttClient.begin(config.mqtt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize MQTT client");
        metricInitStatus.setValue((int32_t)InitStatus::ErrMqtt);
    }

    // nibegw
    err = nibeMqttGw.begin(config.nibe, mqttClient);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Nibe MQTT Gateway");
        metricInitStatus.setValue((int32_t)InitStatus::ErrNibeMqttGw);
    }
    err = nibegw.begin(nibeMqttGw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize Nibe Gateway (RS485)");
        metricInitStatus.setValue((int32_t)InitStatus::ErrNibeGw);
    }

    // energy meter, init mqtt
    err = energyMeter.beginMqtt(mqttClient);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialize energy meter (mqtt)");
        metricInitStatus.setValue((int32_t)InitStatus::ErrEnergyMeterMqtt);
    }

    // relays
    for (uint8_t i = 0; i < RELAY_COUNT; i++) {
        err = relays[i].begin(&mqttClient);
        if (err != 0) {
            ESP_LOGE(TAG, "Could not initialize relay %d", i);
            metricInitStatus.setValue((int32_t)InitStatus::ErrRelay);
        }
    }

    // start polling task
    // Prios: idle=0, main_app/arduino setup/loop=1, mqtt_logging=4, mqtt=5 (default), polling=10, nibegw=15
    err = xTaskCreatePinnedToCore(&pollingTask, "pollingTask", 4 * 1024, NULL, 10, NULL, 1);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not start polling task");
        metricInitStatus.setValue((int32_t)InitStatus::ErrPollingTask);
    }
}

void resetBootCounter() {
    esp_err_t err = nvs_set_u32(nvsHandle, NIBEGW_NVS_KEY_BOOT_COUNT, 0);
    if (err == ESP_OK) {
        err = nvs_commit(nvsHandle);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u32/nvs_commit(%s) failed: %d", NIBEGW_NVS_KEY_BOOT_COUNT, err);
    } else {
        ESP_LOGI(TAG, "Boot counter reset");
    }
    // reset even if nvs write failed to avoid endless write attempts to nvs
    bootCounter = 0;
}

void pollingTask(void* pvParameters) {
    while (1) {
        unsigned long start_time = millis();
        mqttClient.publishAvailability();

        for (uint8_t i = 0; i < RELAY_COUNT; i++) {
            relays[i].publishState();
        }

        energyMeter.publishState();
        nibeMqttGw.publishState();

        // metrics
        metricTotalFreeBytes.setValue(ESP.getFreeHeap());
        metricMinimumFreeBytes.setValue(ESP.getMinFreeHeap());
        metricUptime.setValue(millis() / 1000);

        // reset boot counter after 3 minute, assuming that nibegw is running stable
        if (bootCounter > 0 && metricUptime.getValue() > 180) {
            resetBootCounter();
        }

        // measure runtime and calculate delay
        unsigned long runtime = millis() - start_time;
        metricPollingTime.setValue(runtime);
        if (runtime < 30000) {
            delay(30000 - runtime);
        } else {
            ESP_LOGW(TAG, "Polling task runtime: %lu ms", runtime);
            delay(1000);
        }
    }
}

void loop() {
    if ((InitStatus)metricInitStatus.getValue() == InitStatus::OK && mqttClient.status() == MqttStatus::OK) {
        KMPProDinoESP32.processStatusLed(green, 1000);
    } else {
        KMPProDinoESP32.processStatusLed(red, 1000);
    }
    httpServer.handleClient();
    delay(2);
}
