#ifndef _mqtt_h_
#define _mqtt_h_

#include <Arduino.h>
#include <esp_err.h>
#include <mqtt_client.h>

#define MAX_SUBSCRIPTIONS 10

#define ESP_MQTT_ERROR 0x100
#define ESP_MQTT_DISCONNECTED 0x101

struct MqttConfig {
    String brokerUri;
    String user;
    String password;
    String clientId;

    String rootTopic;
    String discoveryPrefix;
    String hostname;
};

class MqttSubscriptionCallback {
   public:
    virtual void onMqttMessage(const String& topic, const String& payload) = 0;
};

class MqttClientLifecycleCallback {
   public:
    virtual void onConnected() = 0;
    virtual void onDisconnected() = 0;
};

typedef void (MqttSubscriptionCallback::*MqttCallbackFunction)(String, String);

class MqttClient {
   public:
    MqttClient();
    const MqttConfig& getConfig() { return *config; }
    const String& getAvailabilityTopic() { return availabilityTopic; }
    const String& getDeviceDiscoveryInfo() { return deviceDiscoveryInfo; }

    esp_err_t begin(const MqttConfig& config);
    esp_err_t status() { return _status; };

    esp_err_t registerLifecycleCallback(MqttClientLifecycleCallback* callback);

    int publishAvailability();
    int publish(const String& topic, const String& payload, int qos = 0, int retain = 0);
    int subscribe(const String& topic, MqttSubscriptionCallback* callback, int qos = 0);

   private:
    const MqttConfig* config;
    esp_err_t _status;
    String availabilityTopic;
    String deviceDiscoveryInfo;
    esp_mqtt_client_handle_t client;
    MqttClientLifecycleCallback* lifecycleCallbacks[MAX_SUBSCRIPTIONS];
    int lifecycleCallbackCount = 0;
    struct {
        String topic;
        int qos;
        MqttSubscriptionCallback* callback;
    } subscriptions[MAX_SUBSCRIPTIONS];
    int subscriptionCount = 0;

    void onDataEvent(esp_mqtt_event_handle_t event);
    void onConnectedEvent(esp_mqtt_event_handle_t event);
    void onDisconnectedEvent(esp_mqtt_event_handle_t event);

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
};

#endif