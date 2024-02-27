#ifndef _mqtt_h_
#define _mqtt_h_

#include <esp_err.h>
#include <mqtt_client.h>

#include <string>

#define MAX_SUBSCRIPTIONS 10

#define ESP_MQTT_ERROR 0x100
#define ESP_MQTT_DISCONNECTED 0x101

struct MqttConfig {
    std::string brokerUri;
    std::string user;
    std::string password;
    std::string clientId;

    std::string rootTopic;
    std::string discoveryPrefix;
    std::string hostname;

    // do not log for logTopic, would lead to recursion
    std::string logTopic;
};

enum MqttQOS {
    QOS0 = 0,
    QOS1 = 1,
    QOS2 = 2,
};
class MqttSubscriptionCallback {
   public:
    virtual void onMqttMessage(const std::string& topic, const std::string& payload) = 0;
};

class MqttClientLifecycleCallback {
   public:
    virtual void onConnected() = 0;
    virtual void onDisconnected() = 0;
};

typedef void (MqttSubscriptionCallback::*MqttCallbackFunction)(std::string, std::string);

class MqttClient {
   public:
    MqttClient();
    const MqttConfig& getConfig() { return *config; }
    const std::string& getAvailabilityTopic() { return availabilityTopic; }
    const std::string& getDeviceDiscoveryInfo() { return deviceDiscoveryInfo; }

    esp_err_t begin(const MqttConfig& config);
    esp_err_t status() const { return _status; };

    esp_err_t registerLifecycleCallback(MqttClientLifecycleCallback* callback);

    int publishAvailability();
    int publish(const std::string& topic, const std::string& payload, MqttQOS qos = QOS0, bool retain = false);
    int publish(const std::string& topic, const char* payload, MqttQOS qos = QOS0, bool retain = false);
    int publish(const std::string& topic, const char* payload, int length, MqttQOS qos = QOS0, bool retain = false);
    int subscribe(const std::string& topic, MqttSubscriptionCallback* callback, int qos = 0);

   private:
    const MqttConfig* config;
    esp_err_t _status;
    std::string availabilityTopic;
    std::string deviceDiscoveryInfo;
    esp_mqtt_client_handle_t client;
    MqttClientLifecycleCallback* lifecycleCallbacks[MAX_SUBSCRIPTIONS];
    int lifecycleCallbackCount = 0;
    struct {
        std::string topic;
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