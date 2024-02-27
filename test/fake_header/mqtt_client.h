// fake mqtt_client header so that mqtt.h can be compiled

typedef struct {} *esp_mqtt_event_handle_t;
typedef struct {} *esp_mqtt_client_handle_t;

typedef const char*  esp_event_base_t;