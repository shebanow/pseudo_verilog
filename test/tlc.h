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
    // Constructors.
    tlc(const Module* p, const char* str) : Module(p, str) {}
    tlc(const Module* p, const std::string& nm) : Module(p, nm) {}

    // eval function
    void eval() {
        // Reset always takes precedence
        if (!reset_x) {
            // Init registers
            ew_state <= green;
            ns_state <= red;
            timer <= 0;
            ns_cycle <= false;

            // Init outputs
            east_west = green;
            north_south = red;

            // All done this clock.
            return;
        }

        // If we are running a north-south cycle...
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
        }

        // Or an east-west cycle...
        else {
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

        // Drive traffic light outputs.
        north_south = ns_state;
        east_west = ew_state;
    }

    // Ports
    Input<bool> instance(reset_x);
    Input<uint32_t, 8> instance(delay);
    Output<color, 2> instance(east_west);
    Output<color, 2> instance(north_south);

private:
    Register<color, 2> instance(ew_state);
    Register<color, 2> instance(ns_state);
    Register<uint32_t, 8> instance(timer);
    Register<bool> instance(ns_cycle);
};

// TLC test bench
struct tlc_tb : public Testbench {
    // The TLC instance.
    tlc instance(iTLC);

    // Constructors.
    tlc_tb(const std::string& nm) : Testbench(nm) { opt_timer_ticks = 4; }
    tlc_tb(const char* str) : Testbench(str) { opt_timer_ticks = 4; }

    void tlc_usage() {
        extern char* prog_name;
        std::cerr << "usage: " << prog_name << " <program options> tlc [-t timer_ticks]" << std::endl;
        exit(1);
    }

    void main(int argc, char** argv) {
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

        // Set simulation() options.
        set_cycle_limit(cycle_limit);
        set_iteration_limit(10);

        // Do the simulation
        int exit_code;
        try {
            exit_code = simulation();
        } catch (const std::exception& e) {
            std::cerr << "Caught system error: " << e.what() << std::endl;
            exit(1);
        }
        if (exit_code != 0) 
            fprintf(stderr, "Simulation error: %s\n", error_string().c_str());
        else
            printf("TLC passed simulation after %u clocks.\n", run_time());
    }

    // activity around clocks
    void eval() {
        if (!reset_done) {
            reset_done <= true;
            iTLC.delay = opt_timer_ticks - 1;
            iTLC.reset_x = false;
        } else
            iTLC.reset_x = true;
    }

    void post_clock(const uint32_t cycle_num) {
        printf("clock %u: East-West = %s, North-South = %s\n", cycle_num, color2str(iTLC.east_west), color2str(iTLC.north_south));
    }

    // Timer length option.
    int opt_timer_ticks;

    // Reset state machine (simple).
    Register<bool> instance(reset_done, false);
};
