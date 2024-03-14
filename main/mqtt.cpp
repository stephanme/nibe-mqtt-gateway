#include "mqtt.h"

#include <esp_app_desc.h>
#include <esp_log.h>

#include "config.h"

static const char* TAG = "mqtt";

// TODO: hostname and mac address
static const char* DEVICE_DISCOVERY_INFO = R"(
"availability": [{"topic":"%s"}],
"device":{"name":"Nibe GW","identifiers":["%s"],"manufacturer":"KMP Electronics Ltd.","model":"ProDino ESP32","sw_version":"%s","configuration_url":"http://%s.fritz.box"}
)";

MqttClient::MqttClient(Metrics& metrics) : metricMqttStatus(metrics.addMetric(METRIC_NAME_MQTT_STATUS, 1)) {
    metricMqttStatus.setValue((int32_t)MqttStatus::Disconnected);
}

esp_err_t MqttClient::begin(const MqttConfig& config) {
    // check required config
    if (config.brokerUri.length() == 0) {
        ESP_LOGE(TAG, "MQTT broker URI is required");
        return ESP_ERR_INVALID_ARG;
    }
    if (config.clientId.length() == 0) {
        ESP_LOGE(TAG, "MQTT client ID is required");
        return ESP_ERR_INVALID_ARG;
    }
    if (config.rootTopic.length() == 0) {
        ESP_LOGE(TAG, "MQTT root topic is required");
        return ESP_ERR_INVALID_ARG;
    }
    if (config.discoveryPrefix.length() == 0) {
        ESP_LOGE(TAG, "MQTT discovery prefix is required");
        return ESP_ERR_INVALID_ARG;
    }
    if (config.hostname.length() == 0) {
        ESP_LOGE(TAG, "Hostname is required");
        return ESP_ERR_INVALID_ARG;
    }

    this->config = &config;

    availabilityTopic.reserve(config.rootTopic.length() + 13);  // "/availability"
    availabilityTopic = config.rootTopic;
    availabilityTopic += "/availability";

    const esp_app_desc_t* app_desc = esp_app_get_description();
    size_t len = strlen(DEVICE_DISCOVERY_INFO) + availabilityTopic.length() + config.clientId.length() +
                 strlen(app_desc->version) + config.hostname.length() + 1;
    char str[len];
    snprintf(str, len, DEVICE_DISCOVERY_INFO, availabilityTopic.c_str(), config.clientId.c_str(), app_desc->version,
             config.hostname.c_str());
    deviceDiscoveryInfo = str;

    ESP_LOGI(TAG, "MQTT Broker URL: %s", config.brokerUri.c_str());
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = config.brokerUri.c_str();
    mqtt_cfg.credentials.username = config.user.c_str();
    mqtt_cfg.credentials.authentication.password = config.password.c_str();
    mqtt_cfg.credentials.client_id = config.clientId.c_str();
    mqtt_cfg.session.last_will.topic = availabilityTopic.c_str();
    mqtt_cfg.session.last_will.msg = "offline";
    mqtt_cfg.session.last_will.qos = 0;
    mqtt_cfg.session.last_will.retain = 1;

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Could not initialize MQTT client");
        return ESP_FAIL;
    }
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_err_t err = esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, this);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_register_event failed: 0x%x", err);
        return err;
    }
    err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start failed: 0x%x", err);
        return err;
    }
    ESP_LOGI(TAG, "MQTT client started, status=%ld", metricMqttStatus.getValue());

    return ESP_OK;
}

// not thread safe
esp_err_t MqttClient::registerLifecycleCallback(MqttClientLifecycleCallback* callback) {
    if (lifecycleCallbackCount >= MAX_SUBSCRIPTIONS) {
        ESP_LOGE(TAG, "Maximum number of lifecycle callbacks reached");
        return ESP_ERR_NO_MEM;
    }
    lifecycleCallbacks[lifecycleCallbackCount] = callback;
    lifecycleCallbackCount++;

    // initial callback
    if (metricMqttStatus.getValue() == ESP_OK) {
        callback->onConnected();
    } else {
        callback->onDisconnected();
    }
    return ESP_OK;
}

int MqttClient::publishAvailability() { return publish(availabilityTopic, "online", 0, QOS0, true); }

int MqttClient::publish(const std::string& topic, const std::string& payload, MqttQOS qos, bool retain) {
    return publish(topic, payload.c_str(), 0, qos, retain);
}

int MqttClient::publish(const std::string& topic, const char* payload, MqttQOS qos, bool retain) {
    return publish(topic, payload, 0, qos, retain);
}

int MqttClient::publish(const std::string& topic, const char* payload, int length, MqttQOS qos, bool retain) {
    int msg_id = esp_mqtt_client_publish(client, topic.c_str(), payload, length, qos, retain);
    // avoid log checks if not enabled
    if (CONFIG_LOG_MAXIMUM_LEVEL >= ESP_LOG_INFO) {
        if (topic != config->logTopic) {
            if (length == 0) {
                ESP_LOGD(TAG, "publish msg_id=%d, topic=%s, payload=%s", msg_id, topic.c_str(), payload);
            } else {
                ESP_LOGD(TAG, "publish msg_id=%d, topic=%s, payload=%.*s", msg_id, topic.c_str(), length, payload);
            }
        }
    }
    return msg_id;
}

// not thread safe
int MqttClient::subscribe(const std::string& topic, MqttSubscriptionCallback* callback, int qos) {
    if (subscriptionCount >= MAX_SUBSCRIPTIONS) {
        ESP_LOGE(TAG, "Maximum number of subscriptions reached");
        return ESP_ERR_NO_MEM;
    }

    subscriptions[subscriptionCount].topic = topic;
    subscriptions[subscriptionCount].callback = callback;
    subscriptions[subscriptionCount].qos = qos;
    subscriptionCount++;
    int msg_id = esp_mqtt_client_subscribe_single(client, topic.c_str(), qos);
    ESP_LOGI(TAG, "subscribe msg_id=%d, topic=%s", msg_id, topic.c_str());
    return msg_id;
}

void MqttClient::onDataEvent(esp_mqtt_event_handle_t event) {
    // TODO: works only well for small message data that fit into internal buffer (default 1024 bytes)
    // TODO: get rid of extra conversion to std::string?
    std::string topic = std::string((char*)event->topic, event->topic_len);
    std::string data = std::string((char*)event->data, event->data_len);
    for (size_t i = 0; i < subscriptionCount; i++) {
        // TODO: handle wildcard topics
        if (subscriptions[i].topic == topic) {
            subscriptions[i].callback->onMqttMessage(topic, data);
            return;
        }
    }

    ESP_LOGW(TAG, "No callback for topic %s", topic.c_str());
}

void MqttClient::onConnectedEvent(esp_mqtt_event_handle_t event) {
    for (size_t i = 0; i < lifecycleCallbackCount; i++) {
        lifecycleCallbacks[i]->onConnected();
    }

    // re-subscribe
    for (size_t i = 0; i < subscriptionCount; i++) {
        int msg_id = esp_mqtt_client_subscribe_single(client, subscriptions[i].topic.c_str(), subscriptions[i].qos);
        ESP_LOGD(TAG, "re-subscribe msg_id=%d, topic=%s", msg_id, subscriptions[i].topic.c_str());
    }
}

void MqttClient::onDisconnectedEvent(esp_mqtt_event_handle_t event) {
    for (size_t i = 0; i < lifecycleCallbackCount; i++) {
        lifecycleCallbacks[i]->onDisconnected();
    }
}

static void log_error_if_nonzero(const char* message, esp_err_t error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void MqttClient::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    MqttClient* mqttClient = (MqttClient*)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            mqttClient->metricMqttStatus.setValue((int32_t)MqttStatus::OK);
            mqttClient->onConnectedEvent(event);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            mqttClient->metricMqttStatus.setValue((int32_t)MqttStatus::Disconnected);
            mqttClient->onDisconnectedEvent(event);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA TOPIC=%.*s, DATA=%.*s", event->topic_len, event->topic, event->data_len, event->data);
            mqttClient->onDataEvent(event);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
                ESP_LOGE(TAG, "Last errno std::string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            mqttClient->metricMqttStatus.setValue((int32_t)MqttStatus::Error);
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}