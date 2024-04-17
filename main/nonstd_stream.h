// Simple stream like library that supports only unformatted reading.
// Used to work around the ~200k toll for using std::stream on esp32.
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/cplusplus.html#iostream
//
// Implements only the necessary parts needed for this project:
// - reading unformatted data from istream
// - no buffering (unless implicit or provided by the underlying streams)
// - adapter to std::istream for testing on linux target
// - adapter to Arduino Stream (esp32)

#ifndef _nonstd_stream_h_
#define _nonstd_stream_h_

#include <sdkconfig.h>
#include <cstddef>
#include <string>

#if CONFIG_IDF_TARGET_LINUX
// support for std::istream on linux
#include <istream>
#else
#include <LittleFS.h>
#endif

#define EOF (-1)

namespace nonstd {

class istream {
   public:
    typedef unsigned int iostate;
    static const iostate goodbit = 0x0;
    static const iostate badbit = 0x1;
    static const iostate eofbit = 0x2;
    static const iostate failbit = 0x4;

    virtual bool good() const = 0;
    virtual bool eof() const = 0;
    virtual bool bad() const = 0;
    virtual bool fail() const = 0;
    virtual bool operator!() const = 0;
    virtual operator bool() const = 0;
    virtual void clear() = 0;
    virtual void setstate(iostate state) = 0;

    virtual int get() = 0;
};

class istream_base : public istream {
   public:
    istream_base() : state(goodbit) {}

    bool good() const override { return state == goodbit; }
    bool eof() const override { return state & eofbit; }
    bool bad() const override { return state & badbit; }
    bool fail() const override { return state & (failbit | badbit); }
    bool operator!() const override { return fail(); }
    operator bool() const override { return !fail(); }
    void clear() override { state = goodbit; }
    void setstate(iostate state) override { this->state |= state; }

   private:
    iostate state;
};

// istream on a external char buffer (no memory allocation)
class icharbufstream : public istream_base {
   public:
    icharbufstream(const char* str) : icharbufstream(str, strlen(str)) {}
    icharbufstream(const char* str, size_t len) : str(str), pos(0), len(len) {}

    // https://en.cppreference.com/w/cpp/io/basic_istream/get
    int get() override {
        if (!good()) {
            return EOF;
        }
        if (pos >= len) {
            setstate(eofbit | failbit);
            return EOF;
        }
        return str[pos++];
    }

   private:
    const char* str;
    size_t pos;
    size_t len;
};

// lifetime of string must be longer than the stream
class istringstream : public icharbufstream {
   public:
    istringstream(const std::string& str) : icharbufstream(str.c_str(), str.size()) {}

   private:
    // hide char* constructor to avoid implicit conversion to temporary std::string
    istringstream(const char* str) : icharbufstream(str) {}
};

// https://en.cppreference.com/w/cpp/string/basic_string/getline
istream& getline(istream& is, std::string& str);

#if CONFIG_IDF_TARGET_LINUX
// support for std::stream on linux
class istdstream : public istream {
   public:
    istdstream(std::istream& is) : is(is) {}

    bool eof() const override { return is.eof(); }
    bool bad() const override { return is.bad(); }
    bool good() const override { return is.good(); }
    bool fail() const override { return is.fail(); }
    bool operator!() const override { return !is; }
    operator bool() const override { return (bool)is; }
    void clear() override { is.clear(); }
    void setstate(iostate state) override { is.setstate(state); }

    int get() override { return is.get(); }

   private:
    std::istream& is;
};
#else
// adapter for Arduino Stream on esp32
// supports reading only
class arduinostream : public istream_base {
   public:
    arduinostream(Stream& stream) : stream(stream) {}

    // https://en.cppreference.com/w/cpp/io/basic_istream/get
    int get() override {
        if (!good()) {
            return EOF;
        }
        if (stream.available() <= 0) {
            setstate(eofbit | failbit);
            return EOF;
        }
        return stream.read();
    }

   private:
    Stream& stream;
};
#endif

}  // namespace nonstd

#endif  // _nonstd_stream_h_