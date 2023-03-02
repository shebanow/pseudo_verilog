#include <iostream>
#include <string>
#include <algorithm>
#include <vector>
// #include "pv.h"
#include "pv_global.h"

namespace ansi_color_code {
    inline const char* default_font()       { return "\x1b[0m"; }
    inline const char* black_font()         { return "\x1b[30m"; }
    inline const char* red_font()           { return "\x1b[31m"; }
    inline const char* green_font()         { return "\x1b[32m"; }
    inline const char* yellow_font()        { return "\x1b[33m"; }
    inline const char* blue_font()          { return "\x1b[34m"; }
    inline const char* magenta_font()       { return "\x1b[35m"; }
    inline const char* cyan_font()          { return "\x1b[36m"; }
    inline const char* white_font()         { return "\x1b[37m"; }
    inline const char* bold_black_font()    { return "\x1b[30;1m"; }
    inline const char* bold_red_font()      { return "\x1b[31;1m"; }
    inline const char* bold_green_font()    { return "\x1b[32;1m"; }
    inline const char* bold_yellow_font()   { return "\x1b[33;1m"; }
    inline const char* bold_blue_font()     { return "\x1b[34;1m"; }
    inline const char* bold_magenta_font()  { return "\x1b[35;1m"; }
    inline const char* bold_cyan_font()     { return "\x1b[36;1m"; }
    inline const char* bold_white_font()    { return "\x1b[37;1m"; }
}

const char* red() { return ansi_color_code::bold_red_font(); }
const char* green() { return ansi_color_code::bold_green_font(); }
const char* black() { return ansi_color_code::default_font(); }

std::string red_T() { std::string v; v += red(); v+= "T"; v += black(); return v; }
std::string red_F() { std::string v; v += red(); v+= "F"; v += black(); return v; }
std::string green_T() { std::string v; v += green(); v+= "T"; v += black(); return v; }
std::string green_F() { std::string v; v += green(); v+= "F"; v += black(); return v; }

std::vector<int> v {3, -4, 2, -8, 15, 267};

void print_it(int& x) { printf("%d\n", x); }

struct rocket {
    rocket() { printf("in rocket\n"); }
};

struct test {
    test() { printf("in test() constructor\n"); }
    void some_func() { printf("in test::some_func()\n"); }
    global_data_t d;
};

struct sub : public test {
    sub() { this->some_func(); }
    rocket bang;
};

int main(int argc, char** argv) {

    sub x1;
    return 0;
}
