# pick up all sources from main that shall be tested
# must not depend on Arduino
idf_component_register(
    SRCS "main.c" "test_nibegw.cpp" "../main/nibegw.cpp"
    INCLUDE_DIRS "." "../main"
    REQUIRES unity
    WHOLE_ARCHIVE
)
