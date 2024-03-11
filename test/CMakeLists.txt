# pick up all sources from main that shall be tested
# must not depend on Arduino
idf_component_register(
    SRCS "main.c" 
        "test_nibegw.cpp" "../main/nibegw.cpp" 
        "test_nibegw_config.cpp" "../main/nibegw_config.cpp"
        "test_configmgr.cpp" "../main/configmgr.cpp"
        "test_metrics.cpp" "../main/metrics.cpp"
    INCLUDE_DIRS "./fake_header" "../main"
    REQUIRES unity
    WHOLE_ARCHIVE
)
