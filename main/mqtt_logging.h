#ifndef _mqtt_logging_h_
#define _mqtt_logging_h_

#include <freertos/ringbuf.h>

#include "mqtt.h"

// The total number of bytes (not messages) the message buffer will be able to hold at any one time.
#define LOG_BUFFER_SIZE 8 * 1024
// The size, in bytes, required to hold each item in the message,
#define LOG_ITEM_SIZE 256

#define LOG_TASK_PRIORITY 5
#define LOG_TASK_CORE 1

struct LogConfig {
    bool mqttLoggingEnabled;
    bool stdoutLoggingEnabled;
    String logTopic;
};

// singleton, as there can be only one esp_log_set_vprintf
class MqttLogging : MqttClientLifecycleCallback {
   public:
    static esp_err_t begin(const LogConfig &config, MqttClient &mqttClient);

   private:
    MqttLogging() {}
    static MqttLogging instance;

    // configuration
    bool stdoutLoggingEnabled;
    String logTopic;

    MqttClient *mqttClient;
    RingbufHandle_t logEntryRingBuffer;
    TaskHandle_t logTaskHandle;

    virtual void onConnected();
    virtual void onDisconnected();

    static int logging_vprintf(const char *fmt, va_list l);
    static void logTask(void *pvParameters);
};

#endif