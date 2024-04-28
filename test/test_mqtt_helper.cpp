#include <unity.h>

bool mqtt_match_topic(const char* topic, const char* filter);

TEST_CASE("mqtt_match_topic", "[mqtt]") {
    TEST_ASSERT_TRUE(mqtt_match_topic("a/b/c", "a/b/c"));
    TEST_ASSERT_TRUE(mqtt_match_topic("a/b/c", "a/+/c"));
    TEST_ASSERT_TRUE(mqtt_match_topic("a/b/c", "a/#"));
    TEST_ASSERT_TRUE(mqtt_match_topic("a/b/c", "a/+/#"));

    TEST_ASSERT_FALSE(mqtt_match_topic("a/b/d", "a/b/c"));
    TEST_ASSERT_FALSE(mqtt_match_topic("a/b/d", "a/+/c"));
    TEST_ASSERT_FALSE(mqtt_match_topic("b/c/a", "a/#"));

    TEST_ASSERT_FALSE(mqtt_match_topic("a", ""));
    TEST_ASSERT_TRUE(mqtt_match_topic("a", "#"));
    TEST_ASSERT_TRUE(mqtt_match_topic("a", "+"));

    //https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901107
    // empty topic is not allowed -> just for documenting implemented behaviour
    TEST_ASSERT_TRUE(mqtt_match_topic("", ""));
    TEST_ASSERT_FALSE(mqtt_match_topic("", "a"));
    TEST_ASSERT_FALSE(mqtt_match_topic("", "#")); 

    // test cases from spec
    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901241
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis/player1", "sport/tennis/player1/#"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis/player1/ranking", "sport/tennis/player1/#"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis/player1/score/wimbledon", "sport/tennis/player1/#"));

    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis/player1", "sport/tennis/+"));
    TEST_ASSERT_FALSE(mqtt_match_topic("sport/tennis/player1/ranking", "sport/tennis/+"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/", "sport/+"));
    TEST_ASSERT_FALSE(mqtt_match_topic("sport", "sport/+"));
    TEST_ASSERT_FALSE(mqtt_match_topic("sport/", "sport/+/+"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/", "sport/+/#"));
    TEST_ASSERT_FALSE(mqtt_match_topic("sport/tennis", "sport/+/+"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis/", "sport/+/+"));
    TEST_ASSERT_TRUE(mqtt_match_topic("sport/tennis", "sport/+/#"));
    TEST_ASSERT_TRUE(mqtt_match_topic("/finance", "+/+"));
    TEST_ASSERT_TRUE(mqtt_match_topic("/finance", "/+"));
    TEST_ASSERT_FALSE(mqtt_match_topic("/finance", "+"));
}
