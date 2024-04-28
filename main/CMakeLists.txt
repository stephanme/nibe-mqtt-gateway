idf_component_register(
    SRCS "main.cpp" "KMPProDinoESP32.cpp" "MCP23S08.cpp" "configmgr.cpp" "metrics.cpp" "web.cpp" "mqtt.cpp" "mqtt_helper.cpp" "Relay.cpp" "mqtt_logging.cpp" "nibegw.cpp" "nibegw_rs485.cpp" "nibegw_mqtt.cpp" "nibegw_config.cpp" "energy_meter.cpp" "nonstd_stream.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 mqtt nvs_flash
)
target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLOG_LOCAL_LEVEL=ESP_LOG_DEBUG")