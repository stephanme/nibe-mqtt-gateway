idf_component_register(
    SRCS "main.cpp" "KMPProDinoESP32.cpp" "MCP23S08.cpp" "configmgr.cpp" "metrics.cpp" "web.cpp" "mqtt.cpp" "Relay.cpp" "mqtt_logging.cpp" "nibegw.cpp" "nibegw_rs485.cpp" "nibegw_mqtt.cpp" "nibegw_config.cpp" "energy_meter.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 mqtt nvs_flash
)