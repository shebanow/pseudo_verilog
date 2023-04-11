/*
 * Sample pseudo-verilog file - TLC: traffic light controller.
 * Copyright (c) 2023 Michael C Shebanow
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iostream>
#include <cstdint>
#include <string>
#include <getopt.h>
#include "pv.h"

enum color {
    red = 0,
    yellow = 1,
    green = 2
};

const char* color2str(const color c) {
    if (c == red) return "red";
    else if (c == yellow) return "yellow";
    else return "green";
}

class tlc : public Module {
public:
    tlc(const Module* p, const char* str) : tlc(p, std::string(str)) {}
    tlc(const Module* p, const std::string& nm) : Module(p, nm) {}

    // eval function
    void eval() {
        if (ns_cycle) {
            if (ns_state == green) {
                if (timer == 0) {
                    ns_state <= yellow;
                    timer <= delay;
                }
                else timer <= timer - 1;
            }
            else if (ns_state == yellow)
                ns_state <= red;
            else if (ns_state == red) {
                ns_cycle <= false;
                ew_state <= green;
            }
        } else {
            if (ew_state == green) {
                if (timer == 0) {
                    ew_state <= yellow;
                    timer <= delay;
                }
                else timer <= timer - 1;
            }
            else if (ew_state == yellow)
                ew_state <= red;
            else if (ew_state == red) {
                ns_cycle <= true;
                ns_state <= green;
            }
        }
        north_south = ns_state;
        east_west = ew_state;
    }

    // Ports
    Input<uint32_t, 8> instance(delay);
    Output<color, 2> instance(east_west, green);
    Output<color, 2> instance(north_south, red);

private:
    Register<color, 2> instance(ew_state, green);
    Register<color, 2> instance(ns_state, red);
    Register<uint32_t, 8> instance(timer, 0);
    Register<bool> instance(ns_cycle, false);
};

// TLC test bench
struct tlc_tb : public Testbench {
    tlc_tb(const std::string& str) : Testbench(str) {}
    tlc_tb(const char* str) : Testbench(str) {}
    virtual ~tlc_tb() {}

    void tlc_usage() {
        extern char* prog_name;
        std::cerr << "usage: " << prog_name << " <program options> tlc [-t timer_ticks]" << std::endl;
        exit(1);
    }

    void main(int argc, char** argv) {
        int opt_timer_ticks = 4;
        int ch;
        int32_t cycle_limit = 32;

        // Process TLC-specific command line options
        while ((ch = getopt(argc, argv, "+t:")) != -1) {
            switch (ch) {
            case 't':
                opt_timer_ticks = atoi(optarg);
                break;
            default:
                tlc_usage();
                /* NOT REACHED */
                break;
            }
        }
        argc -= optind;
        if (argc) 
            tlc_usage();
        iTLC.delay = opt_timer_ticks - 1;

        // Set simultion() options.
        set_cycle_limit(cycle_limit);
        set_iteration_limit(10);
    }

    // activity around clocks
    void eval() {}
    void post_clock(const uint32_t cycle_num) {
        printf("clock %u: East-West = %s, North-South = %s\n", cycle_num, color2str(iTLC.east_west), color2str(iTLC.north_south));
    }

    // the TLC instance
    tlc instance(iTLC);
};
