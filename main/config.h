// global configuration

#ifndef _config_h_
#define _config_h_

#define ARDUINOJSON_ENABLE_COMMENTS 1

// shared file names (configmgr and web)
#define NIBE_MODBUS_FILE "/nibe_modbus.csv"

// shared metric names (main and web)
#define METRIC_NAME_INIT_STATUS R"(nibegw_status_info{category="init"})"
#define METRIC_NAME_MQTT_STATUS R"(nibegw_status_info{category="mqtt"})"
#define METRIC_NAME_BOOT_COUNT "nibegw_boot_count"

#define NIBEGW_NVS_NAMESPACE "nibegw"

#endif