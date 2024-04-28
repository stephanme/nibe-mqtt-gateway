# pick up all sources from main that shall be tested
# must not depend on Arduino
idf_component_register(
    SRCS "main.c" 
        "test_nibegw.cpp" "../main/nibegw.cpp" 
        "test_nibegw_config.cpp" "../main/nibegw_config.cpp"
        "test_configmgr.cpp" "../main/configmgr.cpp"
        "test_metrics.cpp" "../main/metrics.cpp"
        "test_nonstd_stream.cpp" "../main/nonstd_stream.cpp"
        "test_mqtt_helper.cpp" "../main/mqtt_helper.cpp"
    INCLUDE_DIRS "./fake_header" "../main"
    REQUIRES unity
    WHOLE_ARCHIVE
)
