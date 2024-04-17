#include <unity.h>

#include <sstream>
#include <vector>

#include "nonstd_stream.h"

// https://en.cppreference.com/w/cpp/io/basic_ios/good
template <typename T>
void test_iostate(T& is) {
    is.clear();
    TEST_ASSERT_TRUE(is.good());
    TEST_ASSERT_FALSE(is.fail());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_TRUE(is);
    TEST_ASSERT_FALSE(!is);

    is.clear();
    is.setstate(is.badbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_TRUE(is.bad());
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);

    is.clear();
    is.setstate(is.failbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);

    is.clear();
    is.setstate(is.badbit);
    is.setstate(is.failbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_TRUE(is.bad());
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);

    is.clear();
    is.setstate(is.eofbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_FALSE(is.fail());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_TRUE(is.eof());
    TEST_ASSERT_TRUE(is);
    TEST_ASSERT_FALSE(!is);

    is.clear();
    is.setstate(is.eofbit);
    is.setstate(is.badbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_TRUE(is.bad());
    TEST_ASSERT_TRUE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);

    is.clear();
    is.setstate(is.eofbit);
    is.setstate(is.failbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_TRUE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);

    is.clear();
    is.setstate(is.eofbit);
    is.setstate(is.badbit);
    is.setstate(is.failbit);
    TEST_ASSERT_FALSE(is.good());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_TRUE(is.bad());
    TEST_ASSERT_TRUE(is.eof());
    TEST_ASSERT_FALSE(is);
    TEST_ASSERT_TRUE(!is);
    is.clear();
}

template <typename T>
void test_get(T& is, std::string testString) {
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_FALSE(is.fail());
    TEST_ASSERT_TRUE(is.good());
    for (auto c : testString) {
        TEST_ASSERT_EQUAL(c, is.get());
    }
    TEST_ASSERT_FALSE(is.eof());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_FALSE(is.fail());
    TEST_ASSERT_TRUE(is.good());
    TEST_ASSERT_EQUAL(EOF, is.get());
    TEST_ASSERT_TRUE(is.eof());
    TEST_ASSERT_FALSE(is.bad());
    TEST_ASSERT_TRUE(is.fail());
    TEST_ASSERT_FALSE(is.good());
}

template <typename T>
void test_getline(T& is, std::vector<std::string> testLines) {
    TEST_ASSERT_TRUE(is.good());
    int nLines = 0;
    for (auto l : testLines) {
        std::string line;
        getline(is, line);  // magic: seems to find the correct getline version (std vs nonstd)
        TEST_ASSERT_EQUAL_STRING(l.c_str(), line.c_str());
        nLines++;
    }
    TEST_ASSERT_EQUAL(testLines.size(), nLines);
    TEST_ASSERT_EQUAL(EOF, is.get());
}

// reference test using std::istringstream
TEST_CASE("std::istringstream", "[nonstd_stream]") {
    std::istringstream is("");
    test_iostate(is);
    test_get(is, "");

    std::string testString = "istringstream";
    is = std::istringstream(testString);
    test_get(is, testString);

    is = std::istringstream("");
    test_getline(is, std::vector<std::string>{});
    is = std::istringstream("line1\nline2");
    test_getline(is, std::vector<std::string>{"line1", "line2"});
    is = std::istringstream("\nline1\n\nline2\n");
    test_getline(is, std::vector<std::string>{"", "line1", "", "line2", ""});
}

TEST_CASE("nonstd::icharbufstream", "[nonstd_stream]") {
    nonstd::icharbufstream is("");
    test_iostate(is);
    test_get(is, "");

    const char* testString = "icharbufstream";
    is = nonstd::icharbufstream(testString);
    test_get(is, testString);

    is = nonstd::icharbufstream("");
    test_getline(is, std::vector<std::string>{});
    is = nonstd::icharbufstream("line1\nline2");
    test_getline(is, std::vector<std::string>{"line1", "line2"});
    is = nonstd::icharbufstream("\nline1\n\nline2\n");
    test_getline(is, std::vector<std::string>{"", "line1", "", "line2", ""});
}

TEST_CASE("nonstd::istringstream", "[nonstd_stream]") {
    std::string testString = "";
    nonstd::istringstream is(testString);
    test_iostate(is);
    test_get(is, testString);

    testString = "istringstream";
    is = nonstd::istringstream(testString);
    test_get(is, testString);
}

TEST_CASE("nonstd::istdstream", "[nonstd_stream]") {
    std::stringstream buffer("");
    nonstd::istdstream is(buffer);
    test_iostate(is);
    test_get(is, "");

    std::string testString = "istringstream";
    buffer.clear();
    buffer << testString;
    test_get(is, testString);

    buffer.clear();
    test_getline(is, std::vector<std::string>{});
    buffer.clear();
    buffer << "line1\nline2";
    test_getline(is, std::vector<std::string>{"line1", "line2"});
    buffer.clear();
    buffer << "\nline1\n\nline2\n";
    test_getline(is, std::vector<std::string>{"", "line1", "", "line2", ""});
}