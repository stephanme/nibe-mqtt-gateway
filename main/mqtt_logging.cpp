#include "mqtt_logging.h"

#include <esp_log.h>
#include <esp_system.h>
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

    instance.logEntryRingBuffer = xRingbufferCreate(LOG_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (instance.logEntryRingBuffer == nullptr) {
        ESP_LOGE(TAG, "Could not create logEntryRingBuffer");
        return ESP_FAIL;
    }
    instance.mqttStatusEventGroup = xEventGroupCreate();
    if (instance.mqttStatusEventGroup == nullptr) {
        ESP_LOGE(TAG, "Could not create mqttStatusEventGroup");
        return ESP_FAIL;
    }

    esp_err_t err = xTaskCreatePinnedToCore(logTask, "mqttLoggingTask", 6 * 1024, nullptr, LOG_TASK_PRIORITY,
                                            &instance.logTaskHandle, LOG_TASK_CORE);
    if (err != pdPASS) {
        ESP_LOGE(TAG, "Could not create mqttLoggingTask");
        return ESP_FAIL;
    }

    esp_log_set_vprintf(logging_vprintf);
    ESP_LOGI(TAG, "MQTT logging enabled, topic: %s", instance.logTopic.c_str());
    mqttClient.registerLifecycleCallback(&instance);
    return ESP_OK;
}

int MqttLogging::logging_vprintf(const char *fmt, va_list l) {
    char buffer[LOG_ITEM_SIZE];
    int buffer_len = vsnprintf(buffer, LOG_ITEM_SIZE, fmt, l);
    buffer_len = (buffer_len < LOG_ITEM_SIZE) ? buffer_len : LOG_ITEM_SIZE;
    if (buffer_len > 0) {
        // remove trailing LF
        if (buffer[buffer_len - 1] == '\n') {
            buffer_len--;
            buffer[buffer_len] = '\0';
        }
        // Write to stdout
        if (instance.stdoutLoggingEnabled) {
            puts(buffer);
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

// C wrapper for FreeRTOS task
void MqttLogging::logTask(void *pvParameters) { instance.logTask(); }

// task is suspended on MQTT disconnect -> logs queue up in buffer until full
void MqttLogging::logTask() {
    while (1) {
        // Read from RingBuffer
        size_t item_size = 0;
        char *buffer = (char *)xRingbufferReceive(logEntryRingBuffer, &item_size, portMAX_DELAY);
        if (item_size > 0) {
            // Send to MQTT, one retry on failure
            if (publishLogMsg(buffer, item_size) < 0) {
                // one retry
                vTaskDelay(10 / portTICK_PERIOD_MS);
                if (publishLogMsg(buffer, item_size) < 0) {
                    puts("Publish log failed, dropping");
                }
            }

            vRingbufferReturnItem(logEntryRingBuffer, (void *)buffer);
        }
    }
}

esp_err_t MqttLogging::publishLogMsg(char *msg, int length) {
    // wait for event bit = MQTT connection to broker
    // blocks = new logs can get dropped when ring buffer is full = deliver logs around the network/mqtt outage
    EventBits_t eventBits = xEventGroupWaitBits(mqttStatusEventGroup, MQTT_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    if ((eventBits & MQTT_CONNECTED_BIT) == 0) {
        return ESP_FAIL;
    }
    return instance.mqttClient->publish(logTopic, msg, length);
}

void MqttLogging::onConnected() {
    xEventGroupSetBits(mqttStatusEventGroup, MQTT_CONNECTED_BIT);
    ESP_LOGI(TAG, "MQTT logging resumed");
}

// may be called from logging task
void MqttLogging::onDisconnected() {
    xEventGroupClearBits(mqttStatusEventGroup, MQTT_CONNECTED_BIT);
    ESP_LOGI(TAG, "MQTT logging suspended");
}
