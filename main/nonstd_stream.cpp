#include "nonstd_stream.h"

namespace nonstd {

// https://en.cppreference.com/w/cpp/string/basic_string/getline
istream& getline(istream& is, std::string& str) {
    str.erase();
    int ch;
    while ((ch = is.get()) != EOF) {
        if (ch == '\n') {
            break;
        }
        str.push_back(ch);
    }
    return is;
}

}  // namespace nonstd