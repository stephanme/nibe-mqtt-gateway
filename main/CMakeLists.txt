idf_component_register(
    SRCS "main.cpp" "KMPProDinoESP32.cpp" "MCP23S08.cpp" "config.cpp" "mqtt.cpp" "web.cpp" "Relay.cpp" "mqtt_logging.cpp"
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 mqtt
)
