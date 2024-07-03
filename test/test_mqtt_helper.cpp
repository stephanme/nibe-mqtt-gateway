#include <unity.h>

#include "mqtt_helper.h"

TEST_CASE("matchTopic", "[mqtt]") {
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a/b/c", "a/b/c"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a/b/c", "a/+/c"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a/b/c", "a/#"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a/b/c", "a/+/#"));

    TEST_ASSERT_FALSE(MqttHelper::matchTopic("a/b/d", "a/b/c"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("a/b/d", "a/+/c"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("b/c/a", "a/#"));

    TEST_ASSERT_FALSE(MqttHelper::matchTopic("a", ""));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a", "#"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("a", "+"));

    //https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901107
    // empty topic is not allowed -> just for documenting implemented behaviour
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("", ""));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("", "a"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("", "#")); 

    // test cases from spec
    // https://docs.oasis-open.org/mqtt/mqtt/v5.0/os/mqtt-v5.0-os.html#_Toc3901241
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis/player1", "sport/tennis/player1/#"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis/player1/ranking", "sport/tennis/player1/#"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis/player1/score/wimbledon", "sport/tennis/player1/#"));

    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis/player1", "sport/tennis/+"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("sport/tennis/player1/ranking", "sport/tennis/+"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/", "sport/+"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("sport", "sport/+"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("sport/", "sport/+/+"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/", "sport/+/#"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("sport/tennis", "sport/+/+"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis/", "sport/+/+"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("sport/tennis", "sport/+/#"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("/finance", "+/+"));
    TEST_ASSERT_TRUE(MqttHelper::matchTopic("/finance", "/+"));
    TEST_ASSERT_FALSE(MqttHelper::matchTopic("/finance", "+"));
}

TEST_CASE("mergeMqttDiscoveryInfoOverride", "[mqtt]") {
    JsonDocument doc;
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, "");
    TEST_ASSERT_EQUAL(0, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({})");
    TEST_ASSERT_EQUAL(0, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({"override": null})");
    TEST_ASSERT_EQUAL(0, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({invalid-json-string})");
    TEST_ASSERT_EQUAL(0, doc.size());

    doc["a"] = "b";
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({})");
    TEST_ASSERT_EQUAL(1, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({"override": "o"})");
    TEST_ASSERT_EQUAL_STRING("b", doc["a"]);
    TEST_ASSERT_EQUAL_STRING("o", doc["override"]);
    TEST_ASSERT_EQUAL(2, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({"override": "o1"})");
    TEST_ASSERT_EQUAL_STRING("b", doc["a"]);
    TEST_ASSERT_EQUAL_STRING("o1", doc["override"]);
    TEST_ASSERT_EQUAL(2, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({"override": {"deepOverride":"o1"}})");
    TEST_ASSERT_EQUAL_STRING("b", doc["a"]);
    TEST_ASSERT_EQUAL_STRING("o1", doc["override"]["deepOverride"]);    
    TEST_ASSERT_EQUAL(2, doc.size());
    MqttHelper::mergeMqttDiscoveryInfoOverride(doc, R"({"override": null})");
    TEST_ASSERT_EQUAL_STRING("b", doc["a"]);
    TEST_ASSERT_EQUAL(1, doc.size());
}