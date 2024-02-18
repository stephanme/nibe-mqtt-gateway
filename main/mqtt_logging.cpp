#include "mqtt_logging.h"

#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

static const char *TAG = "mqttlog";

// singleton instance
MqttLogging MqttLogging::instance = MqttLogging();

esp_err_t MqttLogging::begin(const LogConfig &config, MqttClient &mqttClient) {
    if (!config.mqttLoggingEnabled) {
        ESP_LOGI(TAG, "MQTT logging disabled");
        return ESP_OK;
    }

    instance.logTopic = config.logTopic;
    instance.stdoutLoggingEnabled = config.stdoutLoggingEnabled;
    instance.mqttClient = &mqttClient;

    // TODO: RingBuffer vs MessageBuffer?
    instance.logEntryRingBuffer = xRingbufferCreate(LOG_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (instance.logEntryRingBuffer == NULL) {
        ESP_LOGE(TAG, "Could not create ring buffer");
        return ESP_FAIL;
    }
    esp_err_t err = xTaskCreatePinnedToCore(logTask, "mqttLoggingTask", 6 * 1024, nullptr, LOG_TASK_PRIORITY,
                                            &instance.logTaskHandle, LOG_TASK_CORE);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not create task");
        return ESP_FAIL;
    }
    mqttClient.registerLifecycleCallback(&instance);
    if (mqttClient.status() != ESP_OK) {
        instance.onDisconnected();
    }

    esp_log_set_vprintf(logging_vprintf);
    ESP_LOGI(TAG, "MQTT logging enabled, topic: %s", instance.logTopic.c_str());
    return ESP_OK;
}

int MqttLogging::logging_vprintf(const char *fmt, va_list l) {
    char buffer[LOG_ITEM_SIZE];
    int buffer_len = vsnprintf(buffer, LOG_ITEM_SIZE, fmt, l);
    buffer_len = (buffer_len < LOG_ITEM_SIZE) ? buffer_len : LOG_ITEM_SIZE;
    if (buffer_len > 0) {
        // Write to stdout
        if (instance.stdoutLoggingEnabled) {
            fputs(buffer, stdout);
            // append newline when buffer got truncated
            if (buffer[buffer_len - 1] != '\n') {
                putc('\n', stdout);
            }
        }
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        // Send RingBuffer
        BaseType_t sent = xRingbufferSendFromISR(instance.logEntryRingBuffer, &buffer, buffer_len, &xHigherPriorityTaskWoken);
        if (sent != pdTRUE) {
            puts("MQTT logging buffer full");
        }
    }

    return buffer_len;
}

// task is suspended on MQTT disconnect -> logs queue up in buffer until full
void MqttLogging::logTask(void *pvParameters) {
    while (1) {
        // Read from RingBuffer
        size_t item_size = 0;
        char *buffer = (char *)xRingbufferReceive(instance.logEntryRingBuffer, &item_size, portMAX_DELAY);
        if (item_size > 0) {
            // remove trailing LF
            if (buffer[item_size - 1] == '\n') {
                item_size--;
            }
            // Send to MQTT
            instance.mqttClient->publish(instance.logTopic, buffer, item_size);
            vRingbufferReturnItem(instance.logEntryRingBuffer, (void *)buffer);
        }
    }
}

void MqttLogging::onConnected() { vTaskResume(logTaskHandle); }

void MqttLogging::onDisconnected() { vTaskSuspend(logTaskHandle); }
